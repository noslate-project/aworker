'use strict';

require('../common/node_testharness');
const path = require('path');
const { once } = require('events');

const Client = require('../../tools/inspect-client');
const { run } = require('./inspector-util');

const inspectScript = path.join(__dirname, 'fixtures/inspect-target.js');

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
    timeout: 1_000,
  });
  assert_equals(result.type, 'string');
  assert_equals(result.value, 'bar');

  // close client;
  client.reset();
  const [ exitCode ] = await once(cp, 'exit');
  assert_equals(exitCode, 0);
}, 'inspect commands: Runtime.evaluate');
