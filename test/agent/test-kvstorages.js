// META: flags=--expose-internals
'use strict';

const { KvStorage } = load('kv_storage');

promise_test(async () => {
  const storage = await aworker.kvStorages.open('foo');
  assert_true(storage instanceof KvStorage);
}, 'should open new storage');

promise_test(async t => {
  await aworker.kvStorages.open('kv-storages-ns1', {
    evictionStrategy: 'lru',
  });
  await aworker.kvStorages.open('kv-storages-ns1', {
    evictionStrategy: 'lru',
  });
  await promise_rejects_dom(t, DOMException.INVALID_STATE_ERR, aworker.kvStorages.open('kv-storages-ns1'));

  await aworker.kvStorages.open('kv-storages-ns2');
  await aworker.kvStorages.open('kv-storages-ns2');
  await promise_rejects_dom(t, DOMException.INVALID_STATE_ERR, aworker.kvStorages.open('kv-storages-ns2', {
    evictionStrategy: 'lru',
  }));
}, 'should throw invalid states when store config conflicts');
