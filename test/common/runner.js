'use strict';

const fs = require('fs');
const fsPromises = fs.promises;
const path = require('path');
const { inspect } = require('util');
const childProcess = require('child_process');
const Readline = require('readline');
const fixtures = require('./fixtures');
const ResourceServer = require('./resource-server');

class ResourceLoader {
  constructor(path) {
    this.path = path;
  }

  toRealFilePath(from, url) {
    const base = path.dirname(from);
    return url.startsWith('/') ?
      fixtures.path('test', url) :
      fixtures.path('test', base, url);
  }

  /**
   * Load a resource in test/fixtures/wpt specified with a URL
   * @param {string} from the path of the file loading this resource,
   *                      relative to thw WPT folder.
   * @param {string} url the url of the resource being loaded.
   * @param {boolean} asFetch if true, return the resource in a
   *                          pseudo-Response object.
   */
  read(from, url, asFetch = true) {
    const file = this.toRealFilePath(from, url);
    if (asFetch) {
      return fsPromises.readFile(file)
        .then(data => {
          return {
            ok: true,
            json() { return JSON.parse(data.toString()); },
            text() { return data.toString(); },
          };
        });
    }
    return fs.readFileSync(file, 'utf8');
  }
}

class StatusRule {
  constructor(key, value, pattern = undefined) {
    this.key = key;
    this.requires = value.requires || [];
    this.fail = value.fail;
    this.skip = value.skip;
    if (pattern) {
      this.pattern = this.transformPattern(pattern);
    }
    // TODO(joyeecheung): implement this
    this.scope = value.scope;
    this.comment = value.comment;
  }

  /**
   * Transform a filename pattern into a RegExp
   * @param {string} pattern -
   * @return {RegExp} -
   */
  transformPattern(pattern) {
    const result = path.normalize(pattern).replace(/[-/\\^$+?.()|[\]{}]/g, '\\$&');
    return new RegExp(result.replace('*', '.*'));
  }
}

class StatusRuleSet {
  constructor() {
    // We use two sets of rules to speed up matching
    this.exactMatch = {};
    this.patternMatch = [];
  }

  /**
   * @param {object} rules -
   */
  addRules(rules) {
    for (const key of Object.keys(rules)) {
      if (key.includes('*')) {
        this.patternMatch.push(new StatusRule(key, rules[key], key));
      } else {
        this.exactMatch[key] = new StatusRule(key, rules[key]);
      }
    }
  }

  match(file) {
    const result = [];
    const exact = this.exactMatch[file];
    if (exact) {
      result.push(exact);
    }
    for (const item of this.patternMatch) {
      if (item.pattern.test(file)) {
        result.push(item);
      }
    }
    return result;
  }
}

// A specification of the tests
class TestSpec {
  /**
   * @param {string} mod name of the test module, e.g.
   *                     'parallel'
   * @param {string} filename path of the test, relative to mod, e.g.
   *                          'test-error.js'
   * @param {StatusRule[]} rules -
   */
  constructor(mod, filename, rules) {
    this.module = mod;
    this.filename = filename;

    this.requires = new Set();
    this.failReasons = [];
    this.skipReasons = [];
    for (const item of rules) {
      if (item.requires.length) {
        for (const req of item.requires) {
          this.requires.add(req);
        }
      }
      if (item.fail) {
        this.failReasons.push(item.fail);
      }
      if (item.skip) {
        this.skipReasons.push(item.skip);
      }
    }
  }

  getRelativePath() {
    return path.join(this.module, this.filename);
  }

  getAbsolutePath() {
    return fixtures.path('test', this.getRelativePath());
  }

  getContent() {
    return fs.readFileSync(this.getAbsolutePath(), 'utf8');
  }
}

class SpecLoader {
  /**
   * @param {string} path relative path of the test subset
   */
  constructor(path) {
    this.path = path;
    this.loaded = false;
    this.rules = new StatusRuleSet();
    /** @type {TestSpec[]} */
    this.specs = [];
  }

  /**
   * Grep for all test-*.js file recursively in a directory.
   * @param {string} dir -
   */
  grep(dir) {
    let result = [];
    const list = fs.readdirSync(dir);
    for (const file of list) {
      const filepath = path.join(dir, file);
      const stat = fs.statSync(filepath);
      if (stat.isDirectory()) {
        const list = this.grep(filepath);
        result = result.concat(list);
      } else {
        if (!(/test-[\w-_]+\.m?js$/.test(filepath))) {
          continue;
        }
        result.push(filepath);
      }
    }
    return result;
  }

  load() {
    const dir = path.join(fixtures.path('test', this.path));
    const list = this.grep(dir);
    for (const file of list) {
      const relativePath = path.relative(dir, file);
      const match = this.rules.match(relativePath);
      this.specs.push(new TestSpec(this.path, relativePath, match));
    }
    this.loaded = true;
  }
}

const kIntlRequirement = {
  none: 0,
  small: 1,
  full: 2,
};

class IntlRequirement {
  constructor() {
    // TODO(chengzhong.wcz): determine i18n support by inspecting aworker
    this.currentIntl = kIntlRequirement.none;
  }

  /**
   * @param {Set} requires -
   * @return {string|false} The config that the build is lacking, or false
   */
  isLacking(requires) {
    const current = this.currentIntl;
    if (requires.has('full-icu') && current !== kIntlRequirement.full) {
      return 'full-icu';
    }
    if (requires.has('small-icu') && current < kIntlRequirement.small) {
      return 'small-icu';
    }
    return false;
  }
}

const intlRequirements = new IntlRequirement();

const kPass = 'pass';
const kFail = 'fail';
const kSkip = 'skip';
const kTimeout = 'timeout';
const kIncomplete = 'incomplete';
const kUncaught = 'uncaught';
const NODE_UNCAUGHT = 100;

class Runner {
  constructor(path, opt = {}) {
    this.path = path;
    this.resource = opt.resource ?? new ResourceLoader(path);

    this.execArgv = [];
    this.initScript = null;

    this.status = opt.status ?? new SpecLoader(path);
    this.results = {};
    this.inProgress = new Set();
    this.unexpectedFailures = [];

    this.commandMemo = new Map();

    this.printSkipped = opt.printSkipped ?? false;
  }

  load() {
    this.status.load();
    this.specMap = new Map(
      this.status.specs.map(item => [ item.filename, item ])
    );
  }

  /**
   * Sets the exec argv passed to the worker.
   * @param {string[]} execArgv -
   */
  setExecArgv(execArgv) {
    this.execArgv = execArgv;
  }

  /**
   * Sets a script to be run in the worker before executing the tests.
   * @param {string} script -
   */
  setInitScript(script) {
    this.initScript = script;
  }

  /**
   *
   * @param {string[]} queue run subsets of tests
   */
  async runJsTests(queue) {
    if (this.specMap === undefined) {
      this.load();
    }

    if (Array.isArray(queue) && queue.length > 0) {
      queue = queue.map(filename => {
        if (!this.specMap.has(filename)) {
          throw new Error(`${filename} not found!`);
        }
        return this.specMap.get(filename);
      });
    } else {
      queue = this.buildQueue();
    }
    for (const spec of this.specMap.values()) {
      const filename = spec.filename;
      if (queue.find(it => it === spec) === undefined) {
        this.skip(filename, [ 'Skipped in ad-hoc' ]);
        continue;
      }
    }

    this.inProgress = new Set(queue.map(spec => spec.filename));

    for (const spec of queue) {
      const content = spec.getContent();
      const meta = spec.meta = this.getMeta(content);

      const relativePath = spec.getRelativePath();
      const scriptsToRun = [];
      // Scripts specified with the `// META: script=` header
      if (meta.script) {
        for (const script of meta.script) {
          // FIXME(chengzhong.wcz): maybe push
          scriptsToRun.unshift({
            filename: this.resource.toRealFilePath(relativePath, script),
            code: this.resource.read(relativePath, script, false),
          });
        }
      }

      console.log(`---- RUN ${spec.filename} ----`);
      await this.run(spec, scriptsToRun);
    }

    return this.onExit();
  }

  onExit() {
    const total = this.specMap.size;
    if (this.inProgress.size > 0) {
      for (const filename of this.inProgress) {
        this.fail(filename, { name: 'Unknown' }, kIncomplete);
      }
    }
    // TODO(chengzhong.wcz): find a way to pretty print results;
    // inspect.defaultOptions.depth = Infinity;
    // console.log(this.results);

    const failures = [];
    let expectedFailures = 0;
    let skipped = 0;
    for (const key of Object.keys(this.results)) {
      const item = this.results[key];
      if (item.fail && item.fail.unexpected) {
        failures.push(key);
      }
      if (item.fail && item.fail.expected) {
        expectedFailures++;
      }
      if (item.skip) {
        skipped++;
      }
    }
    const ran = total - skipped;
    const passed = ran - expectedFailures - failures.length;
    console.log(`Ran ${ran}/${total} tests, ${skipped} skipped,`,
      `${passed} passed, ${expectedFailures} expected failures,`,
      `${failures.length} unexpected failures`);
    return failures;
  }

  getTestTitle(filename) {
    const spec = this.specMap.get(filename);
    const title = spec.meta && spec.meta.title;
    return title ? `${filename} : ${title}` : filename;
  }

  // Map WPT test status to strings
  getTestStatus(status) {
    switch (status) {
      case 1:
        return kFail;
      case 2:
        return kTimeout;
      case 3:
        return kIncomplete;
      case NODE_UNCAUGHT:
        return kUncaught;
      default:
        return kPass;
    }
  }

  /**
   * Report the status of each specific test case (there could be multiple
   * in one test file).
   *
   * @param {string} filename -
   * @param {Test} test  The Test object returned by WPT harness
   */
  resultCallback(filename, test) {
    const status = this.getTestStatus(test.status);
    const title = this.getTestTitle(filename);
    console.log(`---- ${title} ----`);
    if (status !== kPass) {
      this.fail(filename, test, status);
    } else {
      this.succeed(filename, test, status);
    }
  }

  /**
   * Report the status of each WPT test (one per file)
   *
   * @param {string} filename -
   * @param {object} harnessStatus - The status object returned by WPT harness.
   */
  completionCallback(filename, harnessStatus) {
    // Treat it like a test case failure
    if (harnessStatus.status === 2) {
      const title = this.getTestTitle(filename);
      console.log(`---- ${title} ----`);
      this.resultCallback(filename, { status: 2, name: 'Unknown' });
    }
    this.inProgress.delete(filename);
  }

  addTestResult(filename, item) {
    let result = this.results[filename];
    if (!result) {
      result = this.results[filename] = {};
    }
    if (item.status === kSkip) {
      // { filename: { skip: 'reason' } }
      result[kSkip] = item.reason;
    } else {
      // { filename: { fail: { expected: [ ... ],
      //                      unexpected: [ ... ] } }}
      if (!result[item.status]) {
        result[item.status] = {};
      }
      const key = item.expected ? 'expected' : 'unexpected';
      if (!result[item.status][key]) {
        result[item.status][key] = [];
      }
      if (result[item.status][key].indexOf(item.reason) === -1) {
        result[item.status][key].push(item.reason);
      }
    }
  }

  succeed(filename, test, status) {
    console.log(`[${status.toUpperCase()}] ${test.name}`);
  }

  fail(filename, test, status) {
    const spec = this.specMap.get(filename);
    const expected = !!(spec.failReasons.length);
    if (expected) {
      console.log(`[EXPECTED_FAILURE][${status.toUpperCase()}] ${test.name}`);
      console.log(spec.failReasons.join('; '));
    } else {
      console.log(`[UNEXPECTED_FAILURE][${status.toUpperCase()}] ${test.name}`);
    }
    if (status === kFail || status === kUncaught) {
      console.log(test.message);
      console.log(test.stack);
    }
    const command = this.commandMemo.get(filename);
    console.log(`Command: ${command}\n`);
    this.addTestResult(filename, {
      expected,
      status: kFail,
      reason: test.message || status,
    });
  }

  skip(filename, reasons) {
    const title = this.getTestTitle(filename);
    const joinedReasons = reasons.join('; ');
    if (this.printSkipped) {
      console.log(`---- ${title} ----`);
      console.log(`[SKIPPED] ${joinedReasons}`);
    }
    this.addTestResult(filename, {
      status: kSkip,
      reason: joinedReasons,
    });
  }

  getMeta(code) {
    const matches = code.match(/\/\/ META: .+/g);
    if (!matches) {
      return {};
    }
    const result = {
      env: {},
    };
    for (const match of matches) {
      const parts = match.match(/\/\/ META: ([^=]+?)=(.+)/);
      const key = parts[1];
      const value = parts[2];
      if (key === 'script') {
        if (result[key]) {
          result[key].push(value);
        } else {
          result[key] = [ value ];
        }
      } else if (key.startsWith('env.')) {
        result.env[key.substring(4)] = value;
      } else {
        result[key] = value;
      }
    }
    return result;
  }

  buildQueue() {
    const queue = [];
    for (const spec of this.specMap.values()) {
      const filename = spec.filename;
      if (spec.skipReasons.length > 0) {
        this.skip(filename, spec.skipReasons);
        continue;
      }

      const lackingIntl = intlRequirements.isLacking(spec.requires);
      if (lackingIntl) {
        this.skip(filename, [ `requires ${lackingIntl}` ]);
        continue;
      }

      queue.push(spec);
    }
    return queue;
  }

  async spawn(testFileName, exec, args, options) {
    const command = `${exec} ${args.join(' ')}`;
    this.commandMemo.set(testFileName, command);

    const worker = childProcess.spawn(exec, args, {
      cwd: fixtures.path('tmp'),
      stdio: 'pipe',
      env: options?.env,
    });
    worker.stderr.pipe(process.stderr);

    const readline = Readline.createInterface(worker.stdout);
    const RESULT_ID = '# WORKER - result: ';
    const COMPLETION_ID = '# WORKER - completion: ';
    let completed = false;
    readline.on('line', line => {
      if (!line.startsWith('# WORKER - ')) {
        console.log(line);
        return;
      }
      if (line.startsWith(RESULT_ID)) {
        const result = JSON.parse(line.substring(RESULT_ID.length));
        return this.resultCallback(testFileName, result);
      }
      if (line.startsWith(COMPLETION_ID)) {
        const status = JSON.parse(line.substring(COMPLETION_ID.length));
        status.name = 'COMPLETION';
        completed = true;
        return this.resultCallback(testFileName, status);
      }
    });

    return new Promise(resolve => {
      worker.on('error', err => {
        this.fail(
          testFileName,
          {
            status: NODE_UNCAUGHT,
            name: 'evaluation in Runner.runJsTests()',
            message: err.message,
            stack: inspect(err),
          },
          kUncaught
        );
        this.inProgress.delete(testFileName);
        resolve();
      });

      worker.on('exit', (code, signal) => {
        if (code !== 0 || signal) {
          this.fail(
            testFileName,
            {
              status: NODE_UNCAUGHT,
              name: 'evaluation in Runner.runJsTests()',
              message: `worker process exited with non-zero code ${code} signal ${signal}`,
              stack: '',
            },
            kUncaught
          );
        }
        if (!completed) {
          this.fail(
            testFileName,
            {
              status: kIncomplete,
              name: 'evaluation incomplete',
              message: 'worker process exited without completion callback',
              stack: '',
            },
            kUncaught
          );
        }
        this.inProgress.delete(testFileName);
        resolve();
      });
    });
  }
}

class AworkerRunner extends Runner {
  resourceServer;

  constructor(path, opt) {
    super(path, opt);
    this.resourceServer = new ResourceServer();
  }

  async runJsTests(...args) {
    await this.resourceServer.start();
    return super.runJsTests(...args)
      .finally(() => {
        return this.resourceServer.close();
      });
  }

  /**
   *
   * @param {TestSpec} spec -
   * @param {{filename: string, code: string}[]} scriptsToRun -
   */
  async run(spec, scriptsToRun) {
    const preloadTemplatePath = path.join(__dirname, 'worker/aworker.js');
    const preloadTemplate = fs.readFileSync(preloadTemplatePath, 'utf8');
    const harnessPath = fixtures.path('wpt', 'resources', 'testharness.js');
    const harnessCode = fs.readFileSync(harnessPath, 'utf8');

    const preloadCode = preloadTemplate
      .replace('/** __WORKER_HARNESS_CODE__ */', ';' + harnessCode)
      .replace('/** __WORKER_SCRIPTS_TO_RUN */', `${scriptsToRun.map(it => {
        return `
// MARK: - ${it.filename}
${it.code}
// MARK: END - ${it.filename}
`;
      }).join('\n')}`
      );
    const preloadPath = fixtures.path('tmp', this.path, spec.filename);
    fs.mkdirSync(path.dirname(preloadPath), { recursive: true });
    fs.writeFileSync(preloadPath, preloadCode, 'utf8');

    const execArgv = [
      ...this.execArgv,
      `--preload-script=${preloadPath}`,
    ];
    if (spec.meta.location) {
      execArgv.push(`--location=${spec.meta.location}`);
    }
    if (spec.meta['same-origin-shared-data'] === 'true') {
      const sameOriginSharedData = fixtures.path('tmp', 'caches', this.path, spec.filename);
      execArgv.push(`--same-origin-shared-data=${sameOriginSharedData}`);
      fs.mkdirSync(sameOriginSharedData, { recursive: true });
    }
    if (spec.meta.flags) {
      execArgv.push(...spec.meta.flags.split(' '));
    }

    await super.spawn(spec.filename, fixtures.path('product', 'aworker'), [ ...execArgv, spec.getAbsolutePath() ], {
      env: { ...process.env, ...spec.meta.env },
    });

    this.resourceServer.resetStats();
  }
}

class NodeRunner extends Runner {
  /**
   *
   * @param {TestSpec} spec -
   * @param {{filename: string, code: string}[]} scriptsToRun -
   */
  run(spec, scriptsToRun) {
    return super.spawn(spec.filename, fixtures.path('product', 'node'), [
      ...this.execArgv,
      ...scriptsToRun.flatMap(it => [ '-r', it.filename ]),
      spec.getAbsolutePath(),
    ], {
      env: { ...process.env, ...spec.meta.env },
    });
  }
}

module.exports = {
  StatusRuleSet,
  TestSpec,
  Runner,
  AworkerRunner,
  NodeRunner,
};
