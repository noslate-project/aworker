'use strict';

require('../common/node_testharness');
const path = require('path');
const { once } = require('events');

const Client = require('../../tools/inspect-client');
const { run } = require('./inspector-util');

const inspectScript = path.join(__dirname, 'fixtures/uncaught.js');

promise_test(async () => {
  const cp = await run(inspectScript);

  const client = new Client();
  await client.connect(9229, 'localhost');

  client.callMethod('Runtime.enable');
  client.callMethod('Debugger.enable', { maxScriptsCacheSize: 10000000 });
  client.callMethod('Debugger.setPauseOnExceptions', { state: 'uncaught' });
  client.callMethod('Runtime.runIfWaitingForDebugger');

  const [ event ] = await once(client, 'Debugger.paused');
  assert_equals(event.reason, 'Break at entry');

  const pausedFuture = once(client, 'Debugger.paused');
  await client.callMethod('Debugger.resume');
  {
    const [ event ] = await pausedFuture;
    assert_equals(event.reason, 'exception');
    assert_greater_than(event.callFrames.length, 1);
    assert_equals(event.callFrames[0].functionName, 'foobar');
  }

  // close client;
  client.reset();
  const [ exitCode ] = await once(cp, 'exit');
  assert_equals(exitCode, 1);
}, 'break on uncaught');
