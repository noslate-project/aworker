'use strict';

const common = require('../common/node_testharness');
const path = require('path');
const { once } = require('events');

const Client = require('../../tools/inspect-client');
const { run } = require('./inspector-util');

const inspectScript = path.join(__dirname, 'fixtures/cpu-profiler-target.js');

promise_test(async () => {
  const cp = await run(inspectScript);

  const client = new Client();
  await client.connect(9229, 'localhost');

  await client.callMethod('Debugger.enable');
  await client.callMethod('Profiler.enable');

  const pausedFuture = once(client, 'Debugger.paused');
  await client.callMethod('Runtime.runIfWaitingForDebugger');
  await pausedFuture;

  await client.callMethod('Profiler.start');
  await client.callMethod('Debugger.resume');

  await common.sleep(1000);

  const { profile } = await client.callMethod('Profiler.stop', {
    expression: 'foo',
  });
  assert_greater_than(profile.nodes.length, 0);
  assert_greater_than(profile.startTime, 0);
  assert_greater_than(profile.endTime, 0);

  const scriptNodes = profile.nodes.filter(it => {
    return it.callFrame.url === `file://${inspectScript}`;
  });
  assert_greater_than(scriptNodes.length, 0);
  assert_greater_than(scriptNodes.filter(it => {
    return it.callFrame.functionName === 'main';
  }).length, 0);
  assert_greater_than(scriptNodes.filter(it => {
    return it.callFrame.functionName === 'foo';
  }).length, 0);

  // close client;
  client.reset();
  const [ exitCode ] = await once(cp, 'exit');
  assert_equals(exitCode, 0);
}, 'inspect commands');
