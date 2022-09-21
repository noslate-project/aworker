'use strict';

const path = require('path');
const fs = require('fs').promises;
const common = require('../common/node_testharness');
const fixtures = require('../common/fixtures');
const helper = require('../common/report');

promise_test(async function() {
  const aworkerVersion = await getAworkerVersion();
  let std;
  try {
    await common.execAworker([ '--report', '--report-filename', 'stdout', path.join(__dirname, 'fixtures', 'report-uncaught-exception.js') ], {
      env: {
        ...process.env,
        foo: 'bar',
      },
    });
    assert_unreached('unreachable');
  } catch (e) {
    assert_equals(e.code, 1);
    std = e;
  }

  const stdout = JSON.parse(std.stdout);
  assert_equals(stdout.javascriptStack.message, 'Error: foobar');
  assert_equals(stdout.javascriptStack.errorProperties.foo, 'bar');
  assert_equals(stdout.header.aworkerVersion, aworkerVersion);
}, 'report uncaught exceptions with stdout');

promise_test(async function() {
  const aworkerVersion = await getAworkerVersion();
  let std;
  try {
    await common.execAworker([ '--report', '--report-filename', 'stderr', path.join(__dirname, 'fixtures', 'report-uncaught-exception.js') ], {
      env: {
        ...process.env,
        foo: 'bar',
      },
    });
    assert_unreached('unreachable');
  } catch (e) {
    assert_equals(e.code, 1);
    std = e;
  }

  std.stderr = std.stderr.slice(std.stderr.indexOf('{'));
  const stderr = JSON.parse(std.stderr);
  assert_equals(stderr.javascriptStack.message, 'Error: foobar');
  assert_equals(stderr.javascriptStack.errorProperties.foo, 'bar');
  assert_equals(stderr.header.aworkerVersion, aworkerVersion);
}, 'report uncaught exceptions with stderr');

promise_test(async function() {
  const aworkerVersion = await getAworkerVersion();
  let std;
  const filename = 'report.json';
  try {
    await common.execAworker([ '--report', '--report-filename', path.join(fixtures.path('tmp'), filename), path.join(__dirname, 'fixtures', 'report-uncaught-exception.js') ], {
      cwd: fixtures.path('tmp'),
      env: {
        ...process.env,
        foo: 'bar',
      },
    });
    assert_unreached('unreachable');
  } catch (e) {
    assert_equals(e.code, 1);
    std = e;
  }
  assert_regexp_match(std.stderr, /Writing diagnostic report to file:/);
  assert_regexp_match(std.stderr, /Diagnostic report completed/);
  const fullpath = path.join(fixtures.path('tmp'), filename);
  const stat = await fs.stat(fullpath);
  assert_true(stat.isFile());
  helper.validate(fullpath, [
    [ 'javascriptStack.message', 'Error: foobar' ],
    [ 'javascriptStack.errorProperties.foo', 'bar' ],
    [ 'header.aworkerVersion', aworkerVersion ],
  ]);
}, 'report uncaught exceptions with file');

async function getAworkerVersion() {
  const { stdout } = await common.execAworker([ '--version' ]);
  // strip leading "v".
  return stdout.trim().match(/^v(.+)$/)[1];
}
