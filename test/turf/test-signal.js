'use strict';

require('../common/node_testharness');

const path = require('path');
const events = require('events');
const turf = require('../common/turf');

turf.startTurfDaemon();
promise_test(async function() {
  const cp = await turf.spawnAworker([ path.resolve(__dirname, 'fixtures/echo.js'), '--report-on-signal' ]);

  const [ code ] = await events.once(cp, 'exit');
  assert_equals(code, 0);
  assert_regexp_match(cp.stdout, /foobar/);
}, 'should start with signal watchdog');
