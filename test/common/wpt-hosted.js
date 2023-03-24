'use strict';
const { NoslatedAworkerRunner } = require('./agent-runner');
const { WptHttpd } = require('./wpt');

class WptHostedAworkerRunner extends NoslatedAworkerRunner {
  wptHttpd = new WptHttpd();

  async runJsTests(...args) {
    await this.wptHttpd.start();
    this.execArgv.push(`--location=${this.wptHttpd.wptResourceServerAddr}/test.js`);
    const ret = await super.runJsTests(...args);
    await this.wptHttpd.close();
    return ret;
  }
}

module.exports = {
  WptHostedAworkerRunner,
};
