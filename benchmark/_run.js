/* eslint-env node */
'use strict';

const childProcess = require('child_process');
const path = require('path');
const fs = require('fs');

const fixtures = require('../test/common/fixtures');
const ResourceServer = require('../test/common/resource-server');

class BenchmarkRunner {
  constructor(filepath, options) {
    this.execArgv = [];
    this.options = options;

    this.filepath = filepath;
    this.path = path.join(__dirname, filepath);
    this.code = fs.readFileSync(this.path, 'utf8');

    this.meta = this.getMeta(this.code);
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

  async spawn(exec, args, options) {
    const worker = childProcess.spawn(exec, args, {
      stdio: 'inherit',
      env: options?.env,
    });
    return new Promise((resolve, reject) => {
      worker.on('error', err => {
        reject(err);
      });

      worker.on('exit', (code, signal) => {
        if (code !== 0 || signal) {
          return reject(new Error(`Worker exited with non-zero code: code(${code}), signal(${signal})`));
        }
        resolve();
      });
    });
  }

  runWorker() {
    const workerPath = path.join(__dirname, '_worker.js');
    const workerCode = fs.readFileSync(workerPath, 'utf8');
    const benchmarkCode = fs.readFileSync(path.join(__dirname, '_benchmark.js'), 'utf8');

    const runnerCode = workerCode
      .replace('/** __WORKER_HARNESS_CODE__ */', ';' + benchmarkCode)
      .replace('/** __WORKER_SCRIPTS_TO_RUN */', `
// MARK: - ${this.filename}
${this.code}
// MARK: END - ${this.filename}`
      );
    const runnerPath = path.join(__dirname, '.tmp', this.filepath);
    fs.mkdirSync(path.dirname(runnerPath), { recursive: true });
    fs.writeFileSync(runnerPath, runnerCode, 'utf8');

    const execArgv = [ ...this.execArgv ];
    const benchmarkArgs = [];
    if (this.options.test) {
      benchmarkArgs.push('--test');
    }
    if (this.options.set) {
      benchmarkArgs.push(...this.options.set);
    }
    if (this.meta.flags) {
      execArgv.push(...this.meta.flags.split(' '));
    }

    const env = {
      ...process.env,
      BENCHMARK_ARGV: benchmarkArgs.join(' '),
    };
    return this.spawn(fixtures.path('product', 'aworker'), [ ...execArgv, runnerPath ], {
      env,
    });
  }

  async run() {
    if (this.meta['resource-server']) {
      this.resourceServer = new ResourceServer();
      await this.resourceServer.start();
      this.resourceServer.unref();
    }
    return this.runWorker();
  }
}

module.exports = BenchmarkRunner;
