'use strict';

require('../common/node_testharness');
const path = require('path');
const { once } = require('events');

const Client = require('../../tools/inspect-client');
const { run } = require('./inspector-util');

const inspectScript = path.join(__dirname, 'fixtures/call-and-pause-on-first-statement.js');

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
  {
    const args = await pausedFuture;
    const { callFrames } = args[0];
    assert_equals(callFrames[0].functionName, 'myInnerFunction');
    // Native frames are skipped.
    assert_equals(callFrames[1].functionName, 'myOuterFunction');
  }

  // close client;
  client.reset();
  const [ exitCode ] = await once(cp, 'exit');
  assert_equals(exitCode, 0);
}, 'inspector.callAndPauseOnFirstStatement');

promise_test(async () => {
  const cp = await run(inspectScript, false);

  const client = new Client();
  await client.connect(9229, 'localhost');

  await client.callMethod('Debugger.enable');

  const pausedFuture = once(client, 'Debugger.paused');
  await client.callMethod('Runtime.runIfWaitingForDebugger');
  {
    const args = await pausedFuture;
    const { callFrames } = args[0];
    assert_equals(callFrames[0].functionName, 'myInnerFunction');
    // Native frames are skipped.
    assert_equals(callFrames[1].functionName, 'myOuterFunction');
  }

  // close client;
  client.reset();
  const [ exitCode ] = await once(cp, 'exit');
  assert_equals(exitCode, 0);
}, 'inspector.callAndPauseOnFirstStatement should wait for devtools connection');
