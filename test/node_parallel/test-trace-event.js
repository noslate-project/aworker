'use strict';

const path = require('path');
const os = require('os');
const fs = require('fs');
const common = require('../common/node_testharness');
const { readTraceFile } = require('../common/trace_event');

promise_test(async () => {
  const tmpdir = fs.mkdtempSync(path.join(os.tmpdir(), 'tracing-'));
  const result = await common.execAworker([ '--trace-event-categories=v8.execute', path.join(__dirname, 'fixtures/print-version.js') ], {
    cwd: tmpdir,
  });
  const content = readTraceFile(tmpdir, result.pid);
  assert_true(Array.isArray(content.traceEvents));
  assert_greater_than(content.traceEvents.length, 0);
}, 'should dump v8 trace');

promise_test(async () => {
  const tmpdir = fs.mkdtempSync(path.join(os.tmpdir(), 'tracing-'));
  const result = await common.execAworker([ '--trace-event-categories=aworker', path.join(__dirname, 'fixtures/print-version.js') ], {
    cwd: tmpdir,
  });
  const content = readTraceFile(tmpdir, result.pid);
  assert_true(Array.isArray(content.traceEvents));
  assert_greater_than(content.traceEvents.length, 0);

  assert_true(content.traceEvents.find(it => it.name === 'BootstrapContextPerExecution') != null);

  const versionEvent = content.traceEvents.find(it => it.cat === '__metadata' && it.name === 'version');
  assert_true(versionEvent != null);
  assert_equals(typeof versionEvent.args.aworker, 'string');
}, 'should dump aworker trace');

promise_test(async () => {
  const tmpdir = fs.mkdtempSync(path.join(os.tmpdir(), 'tracing-'));
  await common.execAworker([ '--trace-event-categories=aworker', path.join(__dirname, 'fixtures/fetch.js') ], {
    cwd: tmpdir,
  });
  const content = readTraceFile(tmpdir);
  assert_true(Array.isArray(content.traceEvents));
  assert_greater_than(content.traceEvents.length, 0);

  // TODO(chengzhong.wcz): generalize trace generation in fetch
  // const countEvents = content.traceEvents.filter(it => it.cat.endsWith('aworker.agent_channel.call') && it.name === 'fetch');
  const countEvents = content.traceEvents;
  assert_greater_than(countEvents.length, 0);
}, 'aworker.agent_channel.call,fetch');

promise_test(async () => {
  const tmpdir = fs.mkdtempSync(path.join(os.tmpdir(), 'tracing-'));
  console.log('spawn');
  const cp = common.spawnAworker([ '--trace-event-categories=aworker', path.join(__dirname, 'fixtures/async-loop.js') ], {
    cwd: tmpdir,
  });

  await common.sleep(2000);
  const content = readTraceFile(tmpdir, cp.pid);
  assert_true(Array.isArray(content.traceEvents));
  assert_greater_than(content.traceEvents.length, 0);

  assert_true(content.traceEvents.find(it => it.name === 'BootstrapContextPerExecution') != null);

  const versionEvent = content.traceEvents.find(it => it.cat === '__metadata' && it.name === 'version');
  assert_true(versionEvent != null);
  assert_equals(typeof versionEvent.args.aworker, 'string');

  cp.kill();
}, 'should dump traces with interval');

promise_test(async () => {
  const tmpdir = fs.mkdtempSync(path.join(os.tmpdir(), 'tracing-'));
  console.log('spawn');
  const cp = common.spawnAworker([ '--trace-event-categories=aworker', `--trace-event-directory=${tmpdir}`, path.join(__dirname, 'fixtures/async-loop.js') ]);

  await common.sleep(2000);
  const content = readTraceFile(tmpdir, cp.pid);
  assert_true(Array.isArray(content.traceEvents));
  assert_greater_than(content.traceEvents.length, 0);

  assert_true(content.traceEvents.find(it => it.name === 'BootstrapContextPerExecution') != null);

  const versionEvent = content.traceEvents.find(it => it.cat === '__metadata' && it.name === 'version');
  assert_true(versionEvent != null);
  assert_equals(typeof versionEvent.args.aworker, 'string');

  cp.kill();
}, 'should dump traces to expected directory');
