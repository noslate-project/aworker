'use strict';

const path = require('path');
const os = require('os');
const fs = require('fs');
const events = require('events');
const common = require('../common/node_testharness');
const { readTraceFile, findTraceFile } = require('../common/trace_event');

promise_test(async t => {
  const tmpdir = fs.mkdtempSync(path.join(os.tmpdir(), 'tracing-'));
  const shell = await common.spawnAworkerWithAgent2([ path.join(__dirname, 'fixtures/async-loop.js') ], {
    cwd: tmpdir,
  });

  await events.once(shell.agent, 'diagnostics-bind');
  await shell.agent.tracingStart(shell.credential, [ 'v8', 'aworker' ]);

  await t.step_wait(() => {
    return findTraceFile(tmpdir, shell.cp.pid);
  }, 'wait trace file to be generated', 10_000);
  const content = readTraceFile(tmpdir, shell.cp.pid);
  assert_true(Array.isArray(content.traceEvents));
  assert_greater_than(content.traceEvents.length, 0);

  assert_true(content.traceEvents.find(it => it.name === 'TickTaskQueue') != null);

  const versionEvent = content.traceEvents.find(it => it.cat === '__metadata' && it.name === 'version');
  assert_true(versionEvent != null);
  assert_equals(typeof versionEvent.args.aworker, 'string');

  // TODO: proper resource management.
  shell.cp.kill();
  shell.close();
}, 'should dump trace when dynamically enabled');
