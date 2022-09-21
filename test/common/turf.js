'use strict';

const childProcess = require('child_process');
const path = require('path');
const fs = require('fs');
const EventEmitter = require('events');
const fixtures = require('./fixtures');

const TURF_BIN = fixtures.path('project', 'turf/build/turf');
const TURF_SO = fixtures.path('project', 'turf/build/libturf.so');
const TURF_WORKDIR = fixtures.path('tmp', 'turf');
const SPEC_TEMPLATE = fs.readFileSync(path.resolve(__dirname, 'turf-spec.json'), 'utf8');
const TurfStateLineMatcher = /(\S+):\s+(\S+)/;

let turfDaemon;
function startTurfDaemon() {
  fs.rmSync(fixtures.path('tmp', 'turf/overlay'), { recursive: true, force: true });
  fs.rmSync(fixtures.path('tmp', 'turf/sandbox'), { recursive: true, force: true });

  fs.mkdirSync(fixtures.path('tmp', 'turf/overlay'), { recursive: true });
  fs.mkdirSync(fixtures.path('tmp', 'turf/sandbox'), { recursive: true });
  fs.mkdirSync(fixtures.path('tmp', 'turf/runtime'), { recursive: true });
  fs.mkdirSync(fixtures.path('tmp', 'turf/runtime/aworker'), { recursive: true });
  try {
    fs.unlinkSync(fixtures.path('tmp', 'turf/runtime/aworker/aworker'));
  } catch { /** ignore */ }
  fs.symlinkSync(fixtures.path('product', 'aworker'), fixtures.path('tmp', 'turf/runtime/aworker/aworker'));

  try {
    fs.unlinkSync(fixtures.path('tmp', 'turf/libturf.so'));
  } catch { /** ignore */ }
  fs.symlinkSync(TURF_SO, fixtures.path('tmp', 'turf/libturf.so'));

  turfDaemon = childProcess.spawn(TURF_BIN, [ '-D', '-f' ], {
    env: {
      ...process.env,
      TURF_WORKDIR,
    },
    stdio: [ 'ignore', 'pipe', 'pipe' ],
  });
  turfDaemon.unref();
  turfDaemon.stdout.unref();
  turfDaemon.stderr.unref();

  turfDaemon.on('exit', (code, signal) => {
    console[signal === 'SIGKILL' ? 'warn' : 'error'](`turfd closed with ${code} / ${signal}`);
  });

  let data = '';
  turfDaemon.stdout.on('data', chunk => {
    chunk = chunk.toString();
    for (let i = 0; i < chunk.length; i++) {
      if (chunk[i] === '\n') {
        if (!data.startsWith('tick =')) console.info(data);
        data = '';
      } else {
        data += chunk[i];
      }
    }
  });

  let errData = '';
  turfDaemon.stderr.on('data', chunk => {
    chunk = chunk.toString();
    for (let i = 0; i < chunk.length; i++) {
      if (chunk[i] === '\n') {
        console.error(errData);
        errData = '';
      } else {
        errData += chunk[i];
      }
    }
  });
}

function stopTurfDaemon() {
  if (turfDaemon) {
    turfDaemon.kill('SIGKILL');
    turfDaemon = undefined;
  }
}

const exitEvents = [ 'exit', 'SIGTERM', 'SIGINT' ];
exitEvents.forEach(it => {
  process.on(it, () => {
    stopTurfDaemon();
    process.exit();
  });
});

function parseState(ret) {
  const lines = ret.split('\n').filter(l => l);
  if (!lines.length) return null;
  const obj = lines.reduce((obj, line) => {
    const match = TurfStateLineMatcher.exec(line);
    if (match == null) {
      return obj;
    }
    const [ /** match */, name, value ] = match;
    if (name === 'pid' || name.startsWith('stat.') || name.startsWith('rusage.')) {
      obj[name] = Number.parseInt(value);
    } else {
      obj[name] = value;
    }
    return obj;
  }, {});

  return obj;
}

function turfExec(args, cwd) {
  const opt = {
    env: {
      ...process.env,
      TURF_WORKDIR,
    },
    stdio: [ 'ignore', 'pipe', 'pipe' ],
  };
  if (cwd) {
    opt.cwd = cwd;
  }

  const child = childProcess.spawn(TURF_BIN, args, opt);
  const result = {
    stdout: [],
    stderr: [],
  };
  child.stdout.on('data', chunk => {
    result.stdout.push(chunk);
  });
  child.stderr.on('data', chunk => {
    result.stderr.push(chunk);
  });

  return new Promise((resolve, reject) => {
    // Listen on 'close' event instead of 'exit' event to wait stdout and stderr to be closed.
    child.on('close', (code, signal) => {
      const stdout = Buffer.concat(result.stdout).toString('utf8');
      if (code !== 0) {
        const stderr = Buffer.concat(result.stderr).toString('utf8');
        const err = new Error(`Turf exited with non-zero code(${code}, ${signal}): cmd: "${args.join(' ')}" ${stderr}`);
        err.code = code;
        err.signal = signal;
        err.stderr = stderr;
        err.stdout = stdout;
        return reject(err);
      }
      resolve(stdout);
    });
  });
}

async function turfStateWait(containerName, status, callback) {
  const interval = () => {
    turfExec([ 'state', containerName ])
      .then(result => {
        const report = parseState(result);
        if (report.state !== status) {
          setTimeout(interval, 100);
          return;
        }
        callback(null, report);
      });
  };
  setTimeout(interval, 100);
}

async function spawnAworker(argv, opt) {
  const bundlePath = fixtures.path('tmp');
  const containerName = opt?.containerName ?? 'aworker-test';

  const spec = JSON.parse(SPEC_TEMPLATE);
  spec.process.args = [ 'aworker', ...argv ];
  spec.process.env = spec.process.env.concat(Object.entries(opt?.env ?? {}).map((key, value) => `${key}=${value}`));

  if (opt?.asSeed) {
    spec.turf.seed = true;
  }

  const stdoutPath = fixtures.path('tmp', `${containerName}-stdout.log`);
  const stderrPath = fixtures.path('tmp', `${containerName}-stderr.log`);

  const startArgs = [
    '-H', 'start',
    '--stdout', stdoutPath,
    '--stderr', stderrPath,
  ];
  if (opt?.seed) {
    startArgs.push('--seed', opt.seed);
  }
  startArgs.push(containerName);

  const eventEmitter = new EventEmitter();
  eventEmitter.containerName = containerName;

  fs.rmSync(stdoutPath, { force: true });
  fs.rmSync(stderrPath, { force: true });

  await fs.promises.writeFile(path.resolve(bundlePath, 'config.json'), JSON.stringify(spec), 'utf8');
  await turfExec([ 'create', containerName ], bundlePath);
  await turfExec(startArgs, bundlePath);

  eventEmitter.kill = async () => {
    await turfExec([ '-H', 'stop', containerName ]);
  };

  turfStateWait(containerName, 'stopped', (err, report) => {
    // Defer delete to avoid race condition with `turf stop`.
    setTimeout(() => {
      turfExec([ 'delete', containerName ])
        .then(() => {
          eventEmitter.stdout = fs.readFileSync(stdoutPath, 'utf8');
          eventEmitter.stderr = fs.readFileSync(stderrPath, 'utf8');
          eventEmitter.emit('exit', report.exitcode !== undefined ? Number.parseInt(report.exitcode) : undefined, report['killed.signal']);
        }, err => {
          eventEmitter.emit('error', err);
        });
    }, 1000);
  });

  return eventEmitter;
}

async function spawnAworkerWithSeed(file, argv, opt) {
  const seed = await spawnAworker([ '--mode=seed-userland', ...(opt?.seedArgv ?? []), file ], {
    asSeed: true,
    containerName: 'seed',
  });

  await new Promise((resolve, reject) => {
    turfStateWait('seed', 'forkwait', err => {
      if (err) {
        return reject(err);
      }
      resolve();
    });
  });

  const worker = await spawnAworker([ ...argv, file ], {
    seed: 'seed',
  });

  return {
    seed,
    worker,
  };
}

module.exports = {
  startTurfDaemon,
  stopTurfDaemon,
  turfExec,
  spawnAworker,
  spawnAworkerWithSeed,
};
