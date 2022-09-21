'use strict';

const path = require('path');
const { once } = require('events');

const Client = require('../../tools/inspect-client');
const common = require('../common/node_testharness');

promise_test(async () => {
  const shell = await common.spawnAworkerWithAgent2([ path.join(__dirname, 'fixtures/async-loop-debugger.js') ], {
    startInspectorServer: true,
  });

  await once(shell.agent, 'diagnostics-bind');

  /** wait for the process to be idle in loop */
  await common.sleep(200);

  const inspectorStartedFuture = once(shell.agent, 'inspector-started');
  await shell.agent.inspectorStart(shell.credential);
  await inspectorStartedFuture;

  const client = new Client();
  await client.connect(9229, 'localhost');

  await client.callMethod('Debugger.enable');

  const pausedFuture = once(client, 'Debugger.paused');
  await client.callMethod('Runtime.runIfWaitingForDebugger');
  const [ params ] = await pausedFuture;

  assert_equals(params.reason, 'other');

  client.reset();

  // TODO: proper resource management.
  shell.cp.kill();
  shell.close();
}, 'should start inspector when dynamically enabled');
