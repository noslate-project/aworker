'use strict';

const path = require('path');
const events = require('events');
const fs = require('fs').promises;
const readline = require('readline');
const common = require('../common/node_testharness');
const fixtures = require('../common/fixtures');
const helper = require('../common/report');
const { sleepMaybe } = require('../common/node_testharness');

promise_test(async function() {
  const cp = common.spawnAworker([ '--report-on-signal', path.join(__dirname, 'fixtures', 'report-signal.js') ], {
    cwd: fixtures.path('tmp'),
    stdio: [ 'ignore', 'pipe', 'pipe' ],
  });

  const exitFuture = events.once(cp, 'exit');
  const std = common.collectStdout(cp);

  await waitLine(cp.stdout, /started/);
  // Wait for the child to yield to polling.
  await sleepMaybe(10);
  cp.kill('SIGUSR2');
  await waitLine(cp.stderr, /Diagnostic report completed/);

  cp.kill();
  const [ exitCode, signal ] = await exitFuture;
  assert_equals(exitCode, null);
  assert_equals(signal, 'SIGTERM');

  assert_regexp_match(std.stderr, /Writing diagnostic report to file:/);
  assert_regexp_match(std.stderr, /Diagnostic report completed/);
  const [ , filename ] = std.stderr.match(/Writing diagnostic report to file: (report.\d+.\d+.\d+.\d+.json)/);
  const fullpath = path.join(fixtures.path('tmp'), filename);
  const stat = await fs.stat(fullpath);
  assert_true(stat.isFile());
  helper.validate(fullpath, [
    [ 'header.trigger', 'Signal' ],
    [ 'javascriptStack.message', 'No stack, VmState: EXTERNAL' ],
  ]);
}, 'report on signal');

promise_test(async function() {
  const cp = common.spawnAworker([ '--report-on-signal', path.join(__dirname, 'fixtures', 'report-busyloop.js') ], {
    cwd: fixtures.path('tmp'),
    stdio: [ 'ignore', 'pipe', 'pipe' ],
  });

  const exitFuture = events.once(cp, 'exit');
  const std = common.collectStdout(cp);

  await waitLine(cp.stdout, /started/);
  cp.kill('SIGUSR2');
  await waitLine(cp.stderr, /Diagnostic report completed/);

  cp.kill();
  const [ exitCode, signal ] = await exitFuture;
  assert_equals(exitCode, null);
  assert_equals(signal, 'SIGTERM');

  assert_regexp_match(std.stderr, /Writing diagnostic report to file:/);
  assert_regexp_match(std.stderr, /Diagnostic report completed/);
  const [ , filename ] = std.stderr.match(/Writing diagnostic report to file: (report.\d+.\d+.\d+.\d+.json)/);
  const fullpath = path.join(fixtures.path('tmp'), filename);
  const stat = await fs.stat(fullpath);
  assert_true(stat.isFile());
  const report = helper.validate(fullpath, [
    [ 'header.trigger', 'Signal' ],
    [ 'javascriptStack.message', 'Signal' ],
  ]);
  assert_regexp_match(report.javascriptStack.stack[0], /at main \(.+:5:3\)/);
}, 'report busyloop on signal');

function waitLine(stream, expectedLine) {
  return new Promise(resolve => {
    const rl = readline.createInterface(stream);
    rl.on('line', line => {
      if (line.match(expectedLine)) {
        resolve();
      }
    });
  });
}
