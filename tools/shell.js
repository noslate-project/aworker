#!/usr/bin/env node
'use strict';

const help = `Usage: shell.js <script filename>
  -h, --help                 print usage
  --agent-module-path <path> Path to alice agent module
  --bin                      Path to Serverless Worker Executable
  --inspect-brk              Start inspector and pause on JavaScript entry
  --inspect-brk-aworker       Start inspector and pause on aworker JavaScript entry
  --location HREF            Value of 'globalThis.location' used by some web APIs
  <script filename>          script to be run`;

const fs = require('fs');
const path = require('path');
const childProcess = require('child_process');

class AliceAworkerShell {
  agentModulePath;
  agentServerPath;
  daprAdaptorPath;
  aworkerExecutablePath;
  startInspectorServerFlag;
  agent;
  credential = 'foobar';

  constructor({ agentModulePath, agentServerPath, daprAdaptorPath, aworkerExecutablePath, startInspectorServer, argv }) {
    this.agentModulePath = agentModulePath;
    this.agentServerPath = agentServerPath;
    this.daprAdaptorPath = daprAdaptorPath;
    this.aworkerExecutablePath = aworkerExecutablePath;
    this.startInspectorServerFlag = startInspectorServer;
    this.execArgv = [
      '--has-agent',
      '--agent-type=alice',
      `--agent-ipc=${this.agentServerPath}`,
      `--agent-cred=${this.credential}`,
      ...(argv ?? []),
    ];
  }

  async run() {
    await this.startAliceAgent();
    if (this.startInspectorServerFlag) {
      await this.startInspectorServer();
    }
    await this.execAworker();
    await this.close();
  }

  async execAworker() {
    return new Promise(resolve => {
      const cp = this.spawn();
      cp.on('exit', (code, signal) => {
        if (code != null) {
          process.exitCode = code;
          resolve();
        } else if (signal) {
          process.kill(process.pid, signal);
        }
      });
    });
  }

  spawn(options = {}) {
    const cp = childProcess.spawn(this.aworkerExecutablePath, this.execArgv, {
      shell: true,
      stdio: 'inherit',
      ...options,
    });

    let alive = true;
    const signals = [ 'SIGTERM', 'SIGINT' ];
    for (const it of signals) {
      process.on(it, () => {
        if (alive) {
          cp.kill(it);
        }
      });
    }

    cp.on('exit', () => {
      alive = false;
      for (const it of signals) { process.removeAllListeners(it); }
    });

    return cp;
  }

  async startAliceAgent() {
    if (process.env.ALICE_LOG_LEVEL) {
      const { loggers, getPrettySink } = require(path.join(this._agentModulePath, 'build/lib/loggers'));
      loggers.setSink(getPrettySink());
    }
    const { NoslatedDelegateService } = require(path.join(this._agentModulePath, 'build/delegate'));
    this.agent = new NoslatedDelegateService(this.agentServerPath);
    await this.agent.start();
    if (this.daprAdaptorPath) {
      const daprAdaptor = require(this.daprAdaptorPath);
      this.agent.setDaprAdaptor(daprAdaptor);
    }
    this.agent.register(this.credential, { preemptive: true });
    this.agent.on('disconnect', cred => {
      this.agent.register(cred, { preemptive: true });
    });
  }

  async startInspectorServer() {
    const { InspectorAgent } = require(path.join(this._agentModulePath, 'build/diagnostics/inspector_agent'));
    this.inspectorAgent = new InspectorAgent(this.agent);
    await this.inspectorAgent.start();
  }

  async close() {
    if (this.inspectorAgent) {
      await this.inspectorAgent.close();
    }
    if (this.agent) {
      await this.agent.close();
    }
  }

  get _agentModulePath() {
    if (this.agentModulePath.startsWith('/')) {
      return this.agentModulePath;
    }
    if (this.agentModulePath.startsWith('.')) {
      return path.resolve(process.cwd(), this.agentModulePath);
    }
    return this.agentModulePath;
  }
}

module.exports = {
  resolveAlice,
  AliceAworkerShell,
};

/**
 *
 * @param {string[]} argv -
 */
async function main(argv) {
  if (Number(process.versions.node.split('.')[0]) < 12) {
    console.error('Requires Node.js >= v12');
    return process.exit(1);
  }
  let agentModulePath = resolveAlice();
  let agentServerPath = path.join(process.cwd(), 'noslated.sock');
  let daprAdaptorPath;
  let aworkerExecutablePath = 'aworker';
  let startInspectorServer = false;
  const otherArgv = [];
  while (argv.length > 0) {
    switch (argv[0]) {
      case '--agent-module-path': {
        agentModulePath = argv[1];
        argv.shift();
        break;
      }
      case '--agent-server-path': {
        agentServerPath = argv[1];
        argv.shift();
        break;
      }
      case '--dapr-adaptor-path': {
        daprAdaptorPath = path.resolve(argv[1]);
        argv.shift();
        break;
      }
      case '--bin': {
        aworkerExecutablePath = argv[1];
        argv.shift();
        break;
      }
      case '--help': {
        console.log(help);
        return process.exit(0);
      }
      case '--inspect':
      case '--inspect-brk':
      case '--inspect-brk-aworker': {
        startInspectorServer = true;
      }
      // eslint-disable-next no-fallthrough
      default: {
        otherArgv.push(argv[0]);
      }
    }
    argv.shift();
  }
  if (agentModulePath == null) {
    throw new Error('--agent-module-path');
  }
  const runner = new AliceAworkerShell({
    argv: otherArgv,
    agentModulePath,
    agentServerPath,
    daprAdaptorPath,
    aworkerExecutablePath,
    startInspectorServer,
  });
  await runner.run();
}

function resolveAlice() {
  const pkgJsonPath = path.resolve(__dirname, '../package.json');
  try {
    if (fs.statSync(pkgJsonPath).isFile()) {
      const pkg = require(pkgJsonPath);
      if (pkg.name !== 'noslated') {
        throw '';
      }
      return path.resolve(__dirname, '..');
    }
  } catch { /** empty */ }

  process.env.NODE_PATH = [
    path.resolve(__dirname),
    path.resolve(__dirname, '..'),
    path.resolve(__dirname, '../..'),
    `${process.env.NODE_PATH}`,
  ].join(':');
  require('module').Module._initPaths();
  try {
    return require.resolve('noslated', {
      paths: [
      ],
    });
  } catch {
    return 'noslated';
  }
}

if (require.main === module) {
  main(process.argv.slice(2))
    .then(
      () => {},
      err => {
        console.error(err.message);
        process.exit(1);
      }
    );
}
