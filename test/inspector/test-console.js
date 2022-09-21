'use strict';

require('../common/node_testharness');
const path = require('path');
const { once } = require('events');

const Client = require('../../tools/inspect-client');
const { run } = require('./inspector-util');

const inspectScript = path.join(__dirname, 'fixtures/console.js');

promise_test(async () => {
  const cp = await run(inspectScript);

  const client = new Client();
  await client.connect(9229, 'localhost');

  await client.callMethod('Debugger.enable');
  await client.callMethod('Console.enable');

  const pausedFuture = once(client, 'Debugger.paused');
  await client.callMethod('Runtime.runIfWaitingForDebugger');
  await pausedFuture;

  const entryAddedFuture = once(client, 'Console.messageAdded');
  await client.callMethod('Debugger.resume');
  const [ logEntry ] = await entryAddedFuture;

  assert_equals(logEntry.message.source, 'console-api');
  assert_equals(logEntry.message.level, 'log');
  assert_equals(logEntry.message.text, 'hello world [object Object]');
  assert_equals(logEntry.message.url, inspectScript);
  assert_equals(logEntry.message.line, 3);
  assert_equals(logEntry.message.column, 9);

  // close client;
  client.reset();
  const [ exitCode ] = await once(cp, 'exit');
  assert_equals(exitCode, 0);
}, 'Console domain event: Console.messageAdded');

promise_test(async () => {
  const cp = await run(inspectScript);

  const client = new Client();
  await client.connect(9229, 'localhost');

  await client.callMethod('Debugger.enable');
  await client.callMethod('Runtime.enable');

  const pausedFuture = once(client, 'Debugger.paused');
  await client.callMethod('Runtime.runIfWaitingForDebugger');
  await pausedFuture;

  const consoleAPICalledFuture = once(client, 'Runtime.consoleAPICalled');
  await client.callMethod('Debugger.resume');
  const [ invocation ] = await consoleAPICalledFuture;

  assert_equals(invocation.type, 'log');
  assert_equals(invocation.args.length, 2);
  assert_equals(invocation.args[0].type, 'string');
  assert_equals(invocation.args[0].value, 'hello world');
  assert_equals(invocation.args[1].type, 'object');

  assert_greater_than_equal(invocation.stackTrace.callFrames.length, 1);
  assert_equals(invocation.stackTrace.callFrames[0].url, `file://${inspectScript}`);

  // close client;
  client.reset();
  const [ exitCode ] = await once(cp, 'exit');
  assert_equals(exitCode, 0);
}, 'Runtime domain event: Runtime.consoleAPICalled');
