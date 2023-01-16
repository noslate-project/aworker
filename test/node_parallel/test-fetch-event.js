'use strict';

const assert = require('assert');
const path = require('path');
const common = require('../common/node_testharness');

promise_test(async () => {
  const shell = await common.spawnAworkerWithAgent2([
    '--ref-agent',
    '--expose-internals',
    path.join(__dirname, 'fixtures/fetch-event-dump.js'),
  ]);
  await shell.waitForBind();

  const resp = await shell.invoke(Buffer.from('hello-world'), {
    url: 'http://my-example-host/foo/bar',
    method: 'POST',
    headers: [
      [ 'x-foo', 'bar' ],
    ],
  });

  assert_equals(resp.status, 200);
  const body = await common.streamToBuffer(resp);
  const json = JSON.parse(body.toString('utf8'));
  assert_equals(json.url, 'http://my-example-host/foo/bar');
  assert_equals(json.method, 'POST');
  assert.deepStrictEqual(json.headers, [[ 'x-foo', 'bar' ]]);
  assert.deepStrictEqual(json.baggage, []);
  assert_equals(json.text, 'hello-world');

  // TODO: proper resource management.
  shell.cp.kill('SIGKILL');
  shell.close();
}, 'test fetch event');

promise_test(async () => {
  const shell = await common.spawnAworkerWithAgent2([
    '--ref-agent',
    '--expose-internals',
    path.join(__dirname, 'fixtures/fetch-event-dump.js'),
  ]);
  await shell.waitForBind();

  const resp = await shell.invoke(null, {
    url: 'http://my-example-host/foo/bar',
    method: 'GET',
    headers: [
      [ 'x-foo', 'bar' ],
    ],
    baggage: [
      [ 'x-my-baggage', 'my-baggage-value' ],
    ],
  });

  assert_equals(resp.status, 200);
  const body = await common.streamToBuffer(resp);
  const json = JSON.parse(body.toString('utf8'));
  assert_equals(json.url, 'http://my-example-host/foo/bar');
  assert_equals(json.method, 'GET');
  assert.deepStrictEqual(json.headers, [[ 'x-foo', 'bar' ]]);
  assert.deepStrictEqual(json.baggage, [[ 'x-my-baggage', 'my-baggage-value' ]]);
  assert_equals(json.text, '');

  // TODO: proper resource management.
  shell.cp.kill('SIGKILL');
  shell.close();
}, 'test fetch event baggage');
