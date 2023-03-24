'use strict';
// Flags: --unhandled-rejections=strict

const fs = require('fs').promises;
const path = require('path');
const { NodeRunner, AworkerRunner } = require('../test/common/runner');
const { NoslatedAworkerRunner } = require('../test/common/agent-runner');
const { WPTRunner } = require('../test/common/wpt');
const { PseudoTtyRunner } = require('../test/common/pseudo_tty');
const { WptHostedAworkerRunner } = require('../test/common/wpt-hosted');

const testRoot = path.resolve(__dirname, '../test');

class TestCfg {
  constructor(name, spec) {
    this.name = name;
    this.skip = !!spec.skip;
    this.runner = spec.runner;
    this.execArgv = spec.execArgv;
    this.path = path.resolve(testRoot, name);
  }
}

const runners = {
  'noslated-aworker': NoslatedAworkerRunner,
  aworker: AworkerRunner,
  node: NodeRunner,
  wpt: WPTRunner,
  'pseudo-tty': PseudoTtyRunner,
  'wpt-hosted': WptHostedAworkerRunner,
};

async function main(argv = []) {
  let category;
  let files = [];
  while (argv.length) {
    switch (argv[0]) {
      default: {
        if (category == null) {
          category = argv[0];
          files = argv.slice(1);
        }
      }
    }
    argv.shift();
  }

  const dirs = await fs.readdir(testRoot);
  let specs = [];
  for (const dir of dirs) {
    const stat = await fs.stat(path.join(testRoot, dir));
    if (!stat.isDirectory()) {
      continue;
    }
    const cfgPath = path.join(testRoot, dir, 'testcfg.js');
    try {
      const testcfgStat = await fs.stat(cfgPath);
      if (testcfgStat.isFile()) {
        const cfgData = require(cfgPath);
        const cfg = new TestCfg(dir, cfgData);
        specs.push(cfg);
      }
    } catch {
      /** ignore */
    }
  }

  if (category) {
    const found = specs.find(it => it.name === category);
    if (found == null) {
      throw new Error('Spec not found');
    }
    specs = [ found ];
  }

  const failures = [];
  for (const spec of specs) {
    if (spec.skip) {
      console.log(`# Spec '${spec.name}' was skipped`);
      continue;
    }
    const Runner = runners[spec.runner];
    if (Runner == null) {
      throw new Error(`unrecognizable runner ${spec.runner}`);
    }
    const r = new Runner(spec.name);
    if (spec.execArgv) {
      r.setExecArgv(spec.execArgv);
    }
    r.load();
    if (files.length) {
      failures.push(...await r.runJsTests(files));
    } else {
      failures.push(...await r.runJsTests());
    }
  }
  if (failures.length > 0) {
    process.exitCode = 1;
  }
}

if (require.main === module) {
  const argv = process.argv.slice(2);
  main(argv);
}
