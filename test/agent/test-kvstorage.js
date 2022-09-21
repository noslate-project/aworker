// META: flags=--expose-internals
'use strict';

const { isArrayBuffer } = load('types');

promise_test(async () => {
  const decoder = new TextDecoder();
  const storage = await aworker.kvStorages.open('ns1');
  assert_equals(await storage.get('foo'), undefined);
  assert_equals(await storage.set('foo', 'bar'), undefined);
  assert_equals(decoder.decode(await storage.get('foo')), 'bar');
}, 'should set values');

promise_test(async () => {
  const storage = await aworker.kvStorages.open('ns2');
  assert_array_equals(await storage.list(), []);
  assert_equals(await storage.set('foo', 'bar'), undefined);
  assert_array_equals(await storage.list(), [ 'foo' ]);
}, 'should list values');

promise_test(async () => {
  const decoder = new TextDecoder();
  const storage = await aworker.kvStorages.open('ns3');
  assert_equals(await storage.get('foo'), undefined);

  assert_equals(await storage.set('foo', 'bar'), undefined);
  assert_array_equals(await storage.list(), [ 'foo' ]);
  assert_equals(decoder.decode(await storage.get('foo')), 'bar');

  assert_equals(await storage.delete('foo'), undefined);
  assert_equals(await storage.get('foo'), undefined);
  assert_array_equals(await storage.list(), []);
}, 'should delete values');

promise_test(async t => {
  const storage = await aworker.kvStorages.open('ns4');

  /** assuming the storage max byte length to be 256M */
  await promise_rejects_dom(t, DOMException.QUOTA_EXCEEDED_ERR, storage.set('foo', new Uint8Array(/** 257M/256M */257 * 1024 * 1024)));
}, 'should throw quota exceeded error');

promise_test(async t => {
  const storage = await aworker.kvStorages.open('ns4');

  const key = new Array(1000).fill('a').join('');
  await promise_rejects_js(t, TypeError, storage.set(key, new Uint8Array()));
}, 'should throw TypeError if key exceeded max length');

promise_test(async () => {
  const storage = await aworker.kvStorages.open('ns5', {
    evictionStrategy: 'lru',
  });

  /** assuming the storage max byte length to be 256M */
  await storage.set('foo1', new Uint8Array(/** 100M */100 * 1024 * 1024));
  await storage.set('foo2', new Uint8Array(/** 100M */100 * 1024 * 1024));
  await storage.set('foo3', new Uint8Array(/** 100M */100 * 1024 * 1024));
  await storage.set('foo4', new Uint8Array(/** 100M */100 * 1024 * 1024));
  await storage.set('foo5', new Uint8Array(/** 100M */100 * 1024 * 1024));

  assert_equals(await storage.get('foo1'), undefined);
  assert_equals(await storage.get('foo2'), undefined);

  const result = await storage.get('foo5');
  assert_true(isArrayBuffer(result));
  assert_equals(result.byteLength, /** 100M */100 * 1024 * 1024);
}, 'should evict old keys if exceeding quota in lru mode');
