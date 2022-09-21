'use strict';

require('../common/node_testharness');

const path = require('path');
const events = require('events');
const turf = require('../common/turf');

turf.startTurfDaemon();
promise_test(async function() {
  const { seed, worker } = await turf.spawnAworkerWithSeed(
    path.resolve(__dirname, 'fixtures/seed.js'),
    [ '--report-on-signal' ]);

  const [ code ] = await events.once(worker, 'exit');
  assert_equals(code, 0);
  assert_regexp_match(worker.stdout, /^deserialize\ninstall\n$/);

  seed.kill();
  // seed can be killed with sigkill.
  await events.once(seed, 'exit');
  assert_regexp_match(seed.stdout, /first frame\n$/);
}, 'should start with signal watchdog');
