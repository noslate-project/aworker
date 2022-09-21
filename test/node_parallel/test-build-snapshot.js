'use strict';

const path = require('path');
const common = require('../common/node_testharness');
const fixtures = require('../common/fixtures');

promise_test(async () => {
  await common.execAworker([
    '--build-snapshot',
    '--snapshot-blob', fixtures.path('tmp', 'snapshot.blob'),
    path.join(__dirname, '../fixtures/snapshot/build-snapshot.js'),
  ]);
  const result = await common.execAworker([
    '--snapshot-blob', fixtures.path('tmp', 'snapshot.blob'),
  ]);
  assert_regexp_match(result.stdout, /BUILD-SNAPSHOT: deserialized,deserialization finished,install,installation finished\n/);
}, 'resurrect from snapshot_blob');

promise_test(async () => {
  await common.execAworker([
    '--build-snapshot',
    '--snapshot-blob', fixtures.path('tmp', 'snapshot.blob'),
    path.join(__dirname, '../fixtures/snapshot/build-snapshot-microtask-queue.js'),
  ]);
  const result = await common.execAworker([
    '--snapshot-blob', fixtures.path('tmp', 'snapshot.blob'),
  ]);
  assert_regexp_match(result.stdout, /BUILD-SNAPSHOT: deserialized\n/);
}, 'drain microtask queue on building snapshot');

promise_test(async function testFetch() {
  await common.setupResourceServer(this);

  await common.execAworkerWithAgent([
    '--build-snapshot',
    '--snapshot-blob', fixtures.path('tmp', 'snapshot.blob'),
    path.join(__dirname, '../fixtures/snapshot/build-snapshot-fetch.js'),
  ]);

  const result = await common.execAworkerWithAgent([
    '--snapshot-blob', fixtures.path('tmp', 'snapshot.blob'),
  ]);
  assert_regexp_match(result.stdout, /BUILD-SNAPSHOT: deserialized\n/);
}, 'allow fetch on building snapshot');

promise_test(async function testFetch() {
  await common.execAworker([
    '--build-snapshot',
    '--snapshot-blob', fixtures.path('tmp', 'snapshot.blob'),
    path.join(__dirname, '../fixtures/snapshot/build-snapshot-timer.js'),
  ]);

  const result = await common.execAworker([
    '--snapshot-blob', fixtures.path('tmp', 'snapshot.blob'),
  ]);
  assert_regexp_match(result.stdout, /BUILD-SNAPSHOT: deserialized\n/);
}, 'allow timers on building snapshot');

promise_test(async function testFetch() {
  const result = await common.execAworker([
    '--snapshot-blob=not-exists.blob',
    path.join(__dirname, '../fixtures/snapshot/fallback.js'),
  ]);
  assert_regexp_match(result.stdout, /This is fallback/);
}, 'fallback when snapshot file not exists');

promise_test(async function testFetch() {
  const result = await common.execAworker([
    `--snapshot-blob=${path.join(__dirname, '../fixtures/snapshot/bad-snapshot.blob')}`,
    path.join(__dirname, '../fixtures/snapshot/fallback.js'),
  ]);
  assert_regexp_match(result.stdout, /This is fallback/);
}, 'fallback when snapshot file not valid');
