'use strict';

const path = require('path');
const common = require('../common/node_testharness');

promise_test(async () => {
  const result = await common.execAworker([
    '--mode=seed-userland',
    path.join(__dirname, '../fixtures/fork/fork-userland.js'),
  ]);
  assert_regexp_match(result.stderr.trim(), /Not run in a warm-forkable environment. You may be running in a testing environment./);
  assert_regexp_match(result.stdout.trim(), /^main,serialize,serialized,deserialize,deserialized,install,installed$/);
}, 'fork events');

promise_test(async () => {
  const result = await common.execAworker([
    '--mode=seed-userland',
    path.join(__dirname, '../fixtures/fork/fork-userland-timer.js'),
  ]);
  assert_regexp_match(result.stderr.trim(), /Not run in a warm-forkable environment. You may be running in a testing environment./);
  assert_regexp_match(result.stdout.trim(), /^main,serialize,deserialize,install,oninterval,installed$/);
}, 'fork with userland timers');

promise_test(async () => {
  const result = await common.execAworkerWithAgent([
    '--mode=seed-userland',
    path.join(__dirname, '../fixtures/fork/fork-userland-unfinished-fetch.js'),
  ]).catch(e => {
    console.log(e);
    throw e;
  });
  assert_regexp_match(result.stderr.trim(), /Not run in a warm-forkable environment. You may be running in a testing environment./);
  assert_regexp_match(result.stdout.trim(), /main,serialize,deserialize,expected-rejection,install,installed/);
}, 'fork with userland unfinished fetch');

promise_test(async () => {
  const result = await common.execAworker([
    '--mode=seed-userland',
    path.join(__dirname, '../fixtures/fork/fork-userland-dangling-timer.js'),
  ]);
  assert_regexp_match(result.stderr.trim(), /Not run in a warm-forkable environment. You may be running in a testing environment./);
  assert_regexp_match(result.stdout.trim(), /^main,serialize,deserialize,install$/);
}, 'fork with dangling timer');
