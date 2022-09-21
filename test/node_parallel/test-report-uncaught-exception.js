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
    await common.execAworker([ '--report', path.join(__dirname, 'fixtures', 'report-uncaught-exception.js') ], {
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
  const [ , filename ] = std.stderr.match(/Writing diagnostic report to file: (report.\d+.\d+.\d+.\d+.json)/);
  const fullpath = path.join(fixtures.path('tmp'), filename);
  const stat = await fs.stat(fullpath);
  assert_true(stat.isFile());
  helper.validate(fullpath, [
    [ 'javascriptStack.message', 'Error: foobar' ],
    [ 'javascriptStack.errorProperties.foo', 'bar' ],
    [ 'header.aworkerVersion', aworkerVersion ],
  ]);
}, 'report uncaught exceptions');

promise_test(async function() {
  const aworkerVersion = await getAworkerVersion();
  let std;
  try {
    await common.execAworkerWithAgent([ '--report', path.join(__dirname, 'fixtures', 'report-uncaught-exception.js') ], {
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
  const [ , filename ] = std.stderr.match(/Writing diagnostic report to file: (report.\d+.\d+.\d+.\d+.json)/);
  const fullpath = path.join(fixtures.path('tmp'), filename);
  const stat = await fs.stat(fullpath);
  assert_true(stat.isFile());
  const content = helper.validate(fullpath, [
    [ 'javascriptStack.message', 'Error: foobar' ],
    [ 'javascriptStack.errorProperties.foo', 'bar' ],
    [ 'header.aworkerVersion', aworkerVersion ],
  ]);

  assert_greater_than(content.libuv.length, 0);
  const handle = content.libuv.find(handle => {
    return handle.type === 'pipe';
  });
  assert_equals(handle.is_active, true);
  assert_equals(handle.remoteEndpoint, common.agentSockPath);
}, 'report uncaught exceptions with agent');

async function getAworkerVersion() {
  const { stdout } = await common.execAworker([ '--version' ]);
  // strip leading "v".
  return stdout.trim().match(/^v(.+)$/)[1];
}
