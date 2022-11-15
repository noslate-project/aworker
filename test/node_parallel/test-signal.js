'use strict';

const common = require('../common/node_testharness');
const path = require('path');
const { collectStdout } = require('../common/node_testharness');

promise_test(async () => {
  const cp = common.spawnAworker([ '--mode=seed-userland', path.join(__dirname, 'fixtures', 'exit-signal.js') ], {
    stdio: [ 'ignore', 'pipe', 'pipe' ],
  });

  setTimeout(() => {
    cp.kill('SIGINT');
  }, 500);

  const result = collectStdout(cp);

  await new Promise(resolve => {
    cp.on('exit', () => {
      resolve();
    });
  });

  assert_regexp_match(result.stdout.trim(), /^install,installed,uninstall,uninstalled$/);
}, 'exit with SIGINT');
