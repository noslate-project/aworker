'use strict';

require('../common/node_testharness');

const path = require('path');
const events = require('events');
const turf = require('../common/turf');

turf.startTurfDaemon();

async function testLoopLatency(script, execArgv = []) {
  const startMs = Date.now();
  console.log(script);
  const { seed, worker } = await turf.spawnAworkerWithSeed(path.join(__dirname, script), [ '--loop-latency-limit-ms=1000', ...execArgv ]);

  const { 1: signal } = await events.once(worker, 'exit');
  seed.kill();

  assert_equals(signal, '6' /** SIGABRT */);
  assert_regexp_match(worker.stderr, /FATAL ERROR: LoopLatencyWatchdog Loop latency reached limit/);

  const cost = Date.now() - startMs;
  assert_true(cost < 3000); // 当机器负载高的时候，需要宽容度大一些
  assert_true(cost > 1000);
}

promise_test(async () => {
  await testLoopLatency('fixtures/loop-latency-busy-loop-deserialize.js');
}, 'abort process when loop latency reached limit');
