'use strict';

require('../common/node_testharness');
const path = require('path');
const { once } = require('events');
const childProcess = require('child_process');

const fixtures = require('../common/fixtures');
const Client = require('../../tools/inspect-client');
const { run } = require('./inspector-util');

const inspectScript = path.join(__dirname, 'fixtures/inspect-target.js');

promise_test(async () => {
  const cp = await childProcess.spawn(fixtures.path('product', 'aworker'), [ '--inspect', inspectScript ]);
  const [ code ] = await once(cp, 'exit');
  assert_equals(code, 4);
}, 'should exit if agent is not connected');

promise_test(async () => {
  const cp = await run(inspectScript);

  const client = new Client();
  await client.connect(9229, 'localhost');

  await client.callMethod('Debugger.enable');

  let pausedFuture = once(client, 'Debugger.paused');
  await client.callMethod('Runtime.runIfWaitingForDebugger');
  await pausedFuture;

  pausedFuture = once(client, 'Debugger.paused');
  await client.callMethod('Debugger.resume');
  await pausedFuture;

  const { result } = await client.callMethod('Runtime.evaluate', {
    expression: 'foo',
  });
  assert_equals(result.type, 'string');
  assert_equals(result.value, 'bar');

  // close client;
  client.reset();
  const [ exitCode ] = await once(cp, 'exit');
  assert_equals(exitCode, 0);
}, 'inspect commands');

promise_test(async () => {
  const cp = await run(inspectScript);

  const client = new Client();
  await client.connect(9229, 'localhost');

  await client.callMethod('Debugger.enable');

  const pausedFuture = once(client, 'Debugger.paused');
  await client.callMethod('Runtime.runIfWaitingForDebugger');
  const [ params ] = await pausedFuture;

  assert_equals(params.reason, 'Break at entry');
  // TODO: verify call frame url
  // assert_greater_than_equal(params.callFrames.length, 1);
  // assert_equals(params.callFrames[0].url, `file://${inspectScript}`);

  // close client;
  client.reset();
  const [ exitCode ] = await once(cp, 'exit');
  assert_equals(exitCode, 0);
}, 'Break on entry');
