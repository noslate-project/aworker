'use strict';

const childProcess = require('child_process');
const fixtures = require('./fixtures');
const { AliceAworkerShell, resolveAlice } = require('../../tools/shell');
const ResourceServer = require('./resource-server');
const { buildType } = require('./fixtures');

global.self = global;
require(fixtures.path('wpt', 'resources/testharness.js'));

add_result_callback(result => {
  console.log(`# WORKER - result: ${JSON.stringify({
    status: result.status,
    name: result.name,
    message: result.message,
    stack: result.stack,
  })}`);
});

add_completion_callback((_, status) => {
  console.log(`# WORKER - completion: ${JSON.stringify(status)}`);
});

function spawnAworker(args, options) {
  const cp = childProcess.spawn(fixtures.path('product', 'aworker'), args, options);
  return cp;
}

const agentSockPath = fixtures.path('tmpdir', 'alice.sock');
function spawnAworkerWithAgent(args, options) {
  const cp = childProcess.spawn(process.execPath, [
    fixtures.path('tools', 'shell.js'),
    '--agent-module-path', resolveAlice(),
    '--agent-server-path', agentSockPath,
    '--bin', fixtures.path('product', 'aworker'),
    ...args,
  ], options);
  return cp;
}

function collectStdout(cp) {
  const result = {
    stdout: '',
    stderr: '',
  };
  for (const stdxxx of Object.keys(result)) {
    cp[stdxxx].setEncoding('utf8');
    cp[stdxxx].on('data', chunk => {
      result[stdxxx] += chunk;
    });
  }
  return result;
}

function createExec(fn) {
  return (args, options) => {
    const cp = fn(args, {
      ...(options ?? {}),
      stdio: [ 'ignore', 'pipe', 'pipe' ],
    });
    const result = collectStdout(cp);
    result.pid = cp.pid;

    return new Promise((resolve, reject) => {
      cp.on('error', err => {
        reject(err);
      });
      cp.on('exit', (code, signal) => {
        if (signal != null || code !== 0) {
          const err = new Error(`Child process exited with non-zero code: code(${code}), signal(${signal})`);
          err.code = code;
          err.signal = signal;
          Object.assign(err, result);
          return reject(err);
        }
        resolve(result);
      });
    });
  };
}

function sleep(ms) {
  return new Promise(resolve => {
    setTimeout(resolve, ms);
  });
}

/**
 * Sleep with conditional factors.
 * @param {number} ms
 */
function sleepMaybe(ms) {
  if (buildType !== 'Release') {
    ms *= 100;
  }
  return new Promise(resolve => {
    setTimeout(resolve, ms);
  });
}

async function spawnAworkerWithAgent2(argv, options) {
  const shell = new AliceAworkerShell({
    agentModulePath: resolveAlice(),
    agentServerPath: agentSockPath,
    aworkerExecutablePath: fixtures.path('product', 'aworker'),
    argv,
  });

  await shell.startAliceAgent();
  if (options.startInspectorServer) {
    await shell.startInspectorServer();
  }

  shell.cp = shell.spawn({
    shell: false,
    ...options,
  });
  return shell;
}

async function setupResourceServer(test) {
  const resourceServer = new ResourceServer();
  await resourceServer.start();
  test.add_cleanup(() => {
    return resourceServer.close();
  });
}

module.exports = {
  spawnAworker,
  execAworker: createExec(spawnAworker),
  spawnAworkerWithAgent,
  execAworkerWithAgent: createExec(spawnAworkerWithAgent),
  agentSockPath,
  collectStdout,
  spawnAworkerWithAgent2,

  sleep,
  sleepMaybe,

  setupResourceServer,
};
