'use strict';

/* global process */
const env = typeof aworker === 'object' ? aworker.env : process.env;
const filename = typeof location === 'object' ? location.pathname : 'benchmark';
const exit = typeof aworker === 'object' ? aworker.exit : process.exit;

class Benchmark {
  constructor(fn, configs, options = {}) {
    // Used to make sure a benchmark only start a timer once
    this._startedMap = new Map();

    // Indicate that the benchmark ended
    this._endedMap = new Map();

    // Holds process.hrtime value
    this._timeMap = new Map();

    // Use the file name as the name of the benchmark
    // FIXME: the path content is assumed naive
    this.name = filename.split('/').slice(-2).join('/');

    // Parse job-specific configuration from environ
    // TODO: maybe command line arguments?
    const argv = (env.BENCHMARK_ARGV ?? '').split(' ').filter(it => it !== '');
    const parsed_args = this._parseArgs(argv, configs, options);
    this.options = parsed_args.cli;
    this.extra_options = parsed_args.extra;

    // The configuration list as a queue of jobs
    this.queue = this._queue(this.options);

    // The configuration of the current job, head of the queue
    this.config = this.queue[0];

    this.fn = fn;

    queueMicrotask(() => {
      // _run will run fn for each configuration
      // combination.
      this._run();
    });
  }

  _parseArgs(argv, configs, options) {
    const cliOptions = {};

    // Check for the test mode first.
    const testIndex = argv.indexOf('--test');
    if (testIndex !== -1) {
      for (const [ key, rawValue ] of Object.entries(configs)) {
        let value = Array.isArray(rawValue) ? rawValue[0] : rawValue;
        // Set numbers to one by default to reduce the runtime.
        if (typeof value === 'number') {
          if (key === 'dur' || key === 'duration') {
            value = 0.05;
          } else if (value > 1) {
            value = 1;
          }
        }
        cliOptions[key] = [ value ];
      }
      // Override specific test options.
      if (options.test) {
        for (const [ key, value ] of Object.entries(options.test)) {
          cliOptions[key] = Array.isArray(value) ? value : [ value ];
        }
      }
      argv.splice(testIndex, 1);
    } else {
      // Accept single values instead of arrays.
      for (const [ key, value ] of Object.entries(configs)) {
        if (!Array.isArray(value)) { configs[key] = [ value ]; }
      }
    }

    const extraOptions = {};
    const validArgRE = /^(.+?)=([\s\S]*)$/;
    // Parse configuration arguments
    for (const arg of argv) {
      const match = arg.match(validArgRE);
      if (!match) {
        console.error(`bad argument: ${arg}`);
        exit(1);
      }
      const [ , key, value ] = match;
      if (Object.prototype.hasOwnProperty.call(configs, key)) {
        if (!cliOptions[key]) { cliOptions[key] = []; }
        cliOptions[key].push(
          // Infer the type from the config object and parse accordingly
          typeof configs[key][0] === 'number' ? +value : value
        );
      } else {
        extraOptions[key] = value;
      }
    }
    return { cli: { ...configs, ...cliOptions }, extra: extraOptions };
  }

  _queue(options) {
    const queue = [];
    const keys = Object.keys(options);

    // Perform a depth-first walk through all options to generate a
    // configuration list that contains all combinations.
    function recursive(keyIndex, prevConfig) {
      const key = keys[keyIndex];
      const values = options[key];

      for (const value of values) {
        if (typeof value !== 'number' && typeof value !== 'string') {
          throw new TypeError(
            `configuration "${key}" had type ${typeof value}`);
        }
        if (typeof value !== typeof values[0]) {
          // This is a requirement for being able to consistently and
          // predictably parse CLI provided configuration values.
          throw new TypeError(`configuration "${key}" has mixed types`);
        }

        const currConfig = { [key]: value, ...prevConfig };

        if (keyIndex + 1 < keys.length) {
          recursive(keyIndex + 1, currConfig);
        } else {
          queue.push(currConfig);
        }
      }
    }

    if (keys.length > 0) {
      recursive(0, {});
    } else {
      queue.push({});
    }

    return queue;
  }

  async _run() {
    // TODO: report configuration

    for (const config of this.queue) {
      await new Promise(resolve => this.fn(config, {
        start: () => {
          if (this._startedMap.has(config)) {
            throw new Error('Called start more than once in a single benchmark');
          }
          this._startedMap.set(config, true);
          // TODO: hrtime
          this._timeMap.set(config, Date.now());
        },
        end: operations => {
          // Get elapsed time now and do error checking later for accuracy.
          // TODO: hrtime
          const now = Date.now();

          if (!this._startedMap.get(config)) {
            throw new Error('called end without start');
          }
          if (this._endedMap.get(config)) {
            throw new Error('called end multiple times');
          }
          if (typeof operations !== 'number') {
            throw new Error('called end() without specifying operation count');
          }
          if (!env.BENCHMARK_ZERO_ALLOWED && operations <= 0) {
            throw new Error('called end() with operation count <= 0');
          }

          this._endedMap.set(config, true);
          const time = this._timeMap.get(config);
          const elapsedInSeconds = (now - time) / 1000;
          const rate = operations / elapsedInSeconds;
          this.report(rate, elapsedInSeconds, config);
          resolve();
        },
      }));
    }
  }

  report(rate, elapsedInSeconds, config) {
    sendResult({
      name: this.name,
      conf: config,
      rate,
      time: elapsedInSeconds,
      type: 'report',
    });
  }
}

function formatResult(data) {
  // Construct configuration string, " A=a, B=b, ..."
  let conf = [];
  for (const key of Object.keys(data.conf)) {
    conf.push(`${key}=${JSON.stringify(data.conf[key])}`);
  }
  conf = conf.join(' ');

  let rate = data.rate.toString().split('.');
  rate[0] = rate[0].replace(/(\d)(?=(?:\d\d\d)+(?!\d))/g, '$1,');
  rate = (rate[1] ? rate.join('.') : rate[0]);
  return `"${data.name}", "${conf}", ${data.rate}, ${data.time}`;
}

function sendResult(data) {
  // report by stdout
  console.log(formatResult(data));
}

globalThis.Benchmark = Benchmark;
globalThis.createBenchmark = function createBenchmark(fn, configs, options) {
  return new Benchmark(fn, configs, options);
};
