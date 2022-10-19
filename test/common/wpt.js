'use strict';

const fs = require('fs');
const fsPromises = fs.promises;
const childProcess = require('child_process');
const readline = require('readline');
const path = require('path');
const fixtures = require('./fixtures');
const { StatusRuleSet } = require('./runner');
const { NoslatedAworkerRunner } = require('./agent-runner');

class ResourceLoader {
  toRealFilePath(from, url) {
    // We need to patch this to load the WebIDL parser
    url = url.replace(
      '/resources/WebIDLParser.js',
      '/resources/webidl2/lib/webidl2.js'
    );
    const base = path.dirname(from);
    return url.startsWith('/') ?
      fixtures.path('wpt', url) :
      fixtures.path('wpt', base, url);
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

// A specification of WPT test
class WPTTestSpec {
  /**
   * @param {string} mod name of the WPT module, e.g.
   *                     'html/webappapis/microtask-queuing'
   * @param {string} filename path of the test, relative to mod, e.g.
   *                          'test.any.js'
   * @param {StatusRule[]} rules -
   */
  constructor(mod, filename, rules) {
    this.module = mod;
    this._filename = filename;

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

  get filename() {
    return this.getRelativePath();
  }

  getRelativePath() {
    return path.join(this.module, this._filename);
  }

  getAbsolutePath() {
    return fixtures.path('wpt', this.getRelativePath());
  }

  getContent() {
    return fs.readFileSync(this.getAbsolutePath(), 'utf8');
  }
}

class StatusLoader {
  constructor() {
    this.rules = new StatusRuleSet();
    /** @type {WPTTestSpec[]} */
    this.specs = [];
  }

  /**
   * Grep for all .*.js file recursively in a directory.
   * @param {string} dir -
   * @param {RegExp} rule -
   */
  grep(dir, rule) {
    let result = [];
    const list = fs.readdirSync(dir);
    for (const file of list) {
      const filepath = path.join(dir, file);
      const stat = fs.statSync(filepath);
      if (stat.isDirectory()) {
        const list = this.grep(filepath, rule);
        result = result.concat(list);
      } else {
        if (!(rule.test(filepath))) {
          continue;
        }
        result.push(filepath);
      }
    }
    return result;
  }

  load() {
    const statusFiles = this.grep(fixtures.path('test', 'wpt', 'status'), /\w+\.json$/);
    for (const statusFile of statusFiles) {
      const subset = path.relative(fixtures.path('test', 'wpt', 'status'), statusFile).replace(/\.json$/, '');
      const result = JSON.parse(fs.readFileSync(statusFile, 'utf8'));
      const prefixedRules = Object.fromEntries(Object.entries(result).map(([ match, pattern ]) => [ `${subset}/${match}`, pattern ]));
      this.rules.addRules(prefixedRules);

      const subDir = fixtures.path('wpt', subset);
      const list = this.grep(subDir, /\.\w+\.js$/);
      for (const file of list) {
        const wptRelativePath = path.relative(fixtures.path('wpt'), file);
        const subsetRelativePath = path.relative(subDir, file);
        const match = this.rules.match(wptRelativePath);
        this.specs.push(new WPTTestSpec(subset, subsetRelativePath, match));
      }
    }
  }
}

class WPTRunner extends NoslatedAworkerRunner {
  wptHttpd;
  wptResourceServerAddr = 'http://localhost:8000';
  constructor() {
    super('wpt', {
      resource: new ResourceLoader(),
      status: new StatusLoader(),
    });
  }

  load() {
    this.status.load();
    this.specMap = new Map(
      this.status.specs.map(item => [ item.getRelativePath(), item ])
    );
  }

  async runJsTests(queue) {
    if (this.specMap === undefined) {
      this.load();
    }
    if (queue) {
      queue = this.buildCategorizedQueue(queue);
      if (queue.length === 0) {
        throw new Error('No tests found');
      }
    }
    await this.startWptTestHttpd();
    const ret = await super.runJsTests(queue);
    await this.closeWptTestHttpd();
    return ret;
  }

  buildCategorizedQueue(queue) {
    const result = new Set();
    for (const item of queue) {
      const matcher = new RegExp(item);
      for (const file of this.specMap.keys()) {
        if (matcher.test(file)) {
          result.add(file);
        }
      }
    }
    return [ ...result ];
  }

  async startWptTestHttpd() {
    if (this.wptHttpd) {
      return;
    }
    this.wptHttpd = childProcess.spawn('python',
      [ fixtures.path('wpt', 'wpt'), 'serve', '--config', fixtures.path('tools', 'wpt_serve.json') ],
      {
        stdio: [ 'ignore', 'ignore', 'pipe' ],
      });

    let alive = true;
    const signals = [ 'SIGTERM', 'SIGINT' ];
    const signalHandler = () => {
      if (alive) {
        this.wptHttpd?.kill();
      }
    };
    for (const it of signals) {
      process.on(it, signalHandler);
    }

    this.wptHttpd.on('exit', (code, signal) => {
      alive = false;
      for (const it of signals) {
        process.off(it, signalHandler);
      }
      if (code !== 0) {
        console.error('[wpt] httpd exited with', code, signal);
      }
      this.wptHttpd = null;
    });

    const rl = readline.createInterface(this.wptHttpd.stderr);
    rl.on('line', line => {
      if (line.startsWith('DEBUG')) {
        return;
      }
      console.log(line);
    });
    for await (const line of readline.createInterface(this.wptHttpd.stderr)) {
      const matched = line.match(/INFO:web-platform-tests:Starting http server on web-platform.test:(\d+)/);
      if (matched) {
        this.wptResourceServerAddr = `http://localhost:${matched[1]}`;
        break;
      }
    }
  }

  closeWptTestHttpd() {
    if (this.wptHttpd == null) {
      return;
    }
    console.log('[wpt] shutting down wpt httpd...');
    this.wptHttpd.kill();
  }

  /**
   *
   * @param {WPTTestSpec} spec -
   * @param {{filename: string, code: string}[]} scriptsToRun -
   */
  run(spec, scriptsToRun) {
    const preloadTemplatePath = path.join(__dirname, 'worker/wpt.js');
    const preloadTemplateCode = fs.readFileSync(preloadTemplatePath, 'utf8');
    const harnessPath = fixtures.path('wpt', 'resources', 'testharness.js');
    const idlHarnessPath = fixtures.path('wpt', 'resources', 'idlharness.js');
    const harnessCode = fs.readFileSync(harnessPath, 'utf8');
    const idlHarnessCode = harnessCode + '\n' + fs.readFileSync(idlHarnessPath, 'utf8');

    const preloadCode = preloadTemplateCode
      .replace('/** __WPT_HARNESS_CODE__ */', ';' + (spec.filename.startsWith('idlharness') ? idlHarnessCode : harnessCode))
      .replace('/** __WPT_SCRIPTS_TO_RUN */', `${scriptsToRun.map(it => {
        return `
// MARK: - ${it.filename}
${it.code}
// MARK: END - ${it.filename}
`;
      }).join('\n')}`
      );
    const preloadPath = fixtures.path('tmp', 'wpt', spec.filename);
    fs.mkdirSync(path.dirname(preloadPath), { recursive: true });
    fs.writeFileSync(preloadPath, preloadCode, 'utf8');

    const execArgv = [
      ...this.execArgv,
      `--location=${this.wptResourceServerAddr}/${spec.getRelativePath()}`,
      `--preload-script=${preloadPath}`,
      '--experimental-curl-fetch',
    ];
    return super.spawn(spec.filename, fixtures.path('product', 'aworker'), [ ...execArgv, spec.getAbsolutePath() ]);
  }
}

module.exports = {
  ResourceLoader,
  WPTRunner,
};
