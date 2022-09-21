'use strict';
const path = require('path');
const fs = require('fs').promises;

const common = require('../common/node_testharness');
const fixtures = require('../common/fixtures');
const reportHelper = require('../common/report');

async function testLoopLatency(script, execArgv = []) {
  let err;
  const startMs = Date.now();
  try {
    await common.execAworker([ '--loop-latency-limit-ms=1000', ...execArgv, path.join(__dirname, script) ], {
      cwd: fixtures.path('tmp'),
    });
  } catch (e) {
    err = e;
  }
  const cost = Date.now() - startMs;
  assert_regexp_match(err.stderr, /FATAL ERROR: LoopLatencyWatchdog Loop latency reached limit/);
  assert_true(cost < 3000); // 当机器负载高的时候，需要宽容度大一些
  assert_true(cost > 1000);

  return err;
}

promise_test(async () => {
  await testLoopLatency('fixtures/loop-latency-busy-loop.js');
}, 'abort process when loop latency reached limit');

promise_test(async () => {
  await testLoopLatency('fixtures/loop-latency-first-tick.js');
}, 'abort process when loop latency reached limit at the first tick');

promise_test(async () => {
  await testLoopLatency('fixtures/loop-latency-vm.js');
}, 'abort process when loop latency reached limit in vm busy loop');

promise_test(async () => {
  const std = await testLoopLatency('fixtures/loop-latency-busy-loop.js', [ '--report' ]);

  assert_regexp_match(std.stderr, /Writing diagnostic report to file:/);
  assert_regexp_match(std.stderr, /Diagnostic report completed/);
  const [ , filename ] = std.stderr.match(/Writing diagnostic report to file: (report.\d+.\d+.\d+.\d+.json)/);
  const fullpath = path.join(fixtures.path('tmp'), filename);
  const stat = await fs.stat(fullpath);
  assert_true(stat.isFile());
  reportHelper.validate(fullpath, [
    [ 'header.trigger', 'FatalError' ],
    [ 'header.event', 'Loop latency reached limit' ],
  ]);
}, 'generate report on loop latency reached limit');
