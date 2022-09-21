'use strict';

const { AworkerRunner } = require('./runner');
const fixtures = require('./fixtures');
const path = require('path');

class AliceAworkerRunner extends AworkerRunner {
  agentServerPath;
  agent;

  constructor(path, opt = {}) {
    super(path, opt);
    // use os.tmpdir to prevent concatenating a pathname to be longer than 256 chars.
    this.agentServerPath = opt.agentServerPath ?? fixtures.path('tmpdir', 'aliced.sock');

    this.execArgv = [
      '--has-agent',
      '--agent-type=alice',
      `--agent-ipc=${this.agentServerPath}`,
      '--agent-cred=foobar',
    ];
  }

  async runJsTests(...args) {
    await this.startAliceAgent();
    const ret = await super.runJsTests(...args);
    await this.stopAliceAgent();
    return ret;
  }

  async startAliceAgent() {
    if (process.env.ALICE_LOG_LEVEL) {
      const { loggers, getPrettySink } = require(path.join(fixtures.path('project', 'alice'), 'build/lib/loggers'));
      loggers.setSink(getPrettySink());
    }
    const { AliceDelegateService } = require(path.join(fixtures.path('project', 'alice'), 'build/delegate'));
    this.agent = new AliceDelegateService(this.agentServerPath);
    await this.agent.start();
    this.agent.register('foobar');
    this.agent.on('disconnect', cred => {
      this.agent.register(cred);
    });
  }

  async stopAliceAgent() {
    if (this.agent == null) {
      return;
    }
    await this.agent.close();
  }
}

module.exports = {
  AliceAworkerRunner,
};

if (require.main === module) {
  const runner = new AliceAworkerRunner('alice', {
    agentServerPath: path.join(process.cwd(), 'aliced.sock'),
  });
  runner.startAliceAgent({ ref: true });
  console.log('started at', runner.agentServerPath);
}
