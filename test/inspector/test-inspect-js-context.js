'use strict';

require('../common/node_testharness');
const path = require('path');
const { once } = require('events');

const Client = require('../../tools/inspect-client');
const { run } = require('./inspector-util');

const inspectScript = path.join(__dirname, 'fixtures/js-context.js');

promise_test(async () => {
  const cp = await run(inspectScript);

  const client = new Client();
  await client.connect(9229, 'localhost');

  const contextCreatedFuture1 = once(client, 'Runtime.executionContextCreated');
  client.callMethod('Runtime.enable');
  client.callMethod('Debugger.enable');
  client.callMethod('Runtime.runIfWaitingForDebugger');

  {
    const [ event ] = await contextCreatedFuture1;
    assert_equals(event.context.id, 1);
    assert_equals(event.context.origin, '');
    assert_equals(event.context.name, inspectScript);
    assert_equals(event.context.auxData.isDefault, true);
  }

  {
    const [ event ] = await once(client, 'Debugger.paused');
    assert_equals(event.reason, 'Break at entry');
  }

  const contextCreatedFuture2 = once(client, 'Runtime.executionContextCreated');
  const pausedFuture = once(client, 'Debugger.paused');
  await client.callMethod('Debugger.resume');
  {
    const [ event ] = await pausedFuture;
    assert_equals(event.reason, 'other');
    assert_greater_than(event.callFrames.length, 1);
    assert_equals(event.callFrames[0].functionName, 'myFunction');
    assert_equals(event.callFrames[0].url, 'my-example-script.js');
  }

  {
    const [ event ] = await contextCreatedFuture2;
    assert_equals(event.context.id, 2);
    assert_equals(event.context.origin, 'http://my-example.com');
    assert_equals(event.context.name, 'foobar');
    assert_equals(event.context.auxData.isDefault, false);
  }

  // close client;
  client.reset();
  const [ exitCode ] = await once(cp, 'exit');
  assert_equals(exitCode, 0);
}, 'break on uncaught');
