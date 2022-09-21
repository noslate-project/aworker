// META: same-origin-shared-data=true
'use strict';

async function cleanCaches() {
  const keys = await caches.keys();
  await Promise.all(keys.map(it => caches.delete(it)));
}

promise_test(async () => {
  assert_true(caches instanceof CacheStorage);
  await caches.delete('v1');

  const cache = await caches.open('v1');
  assert_true(cache instanceof Cache);
}, 'cache interface');

promise_test(async () => {
  await cleanCaches();

  const cache = await caches.open('v1');
  await cache.put('http://foobar', new Response('foobar'));
  const resp = await caches.match('http://foobar');
  assert_true(resp != null);
  assert_equals(resp.status, 200);

  const text = await resp.text();
  assert_equals(text, 'foobar');
}, 'CacheStorage.prototype.match');

promise_test(async () => {
  await cleanCaches();

  let test = await caches.has('v1');
  assert_false(test);

  await caches.open('v1');
  test = await caches.has('v1');
  assert_true(test);

  test = await caches.has('v2');
  assert_false(test);
}, 'CacheStorage.prototype.has');

promise_test(async () => {
  await cleanCaches();

  await caches.open('v1');
  let test = await caches.has('v1');
  assert_true(test);

  await caches.delete('v1');
  test = await caches.has('v1');
  assert_false(test);

  test = await caches.has('v2');
  assert_false(test);
}, 'CacheStorage.prototype.delete');

promise_test(async () => {
  await cleanCaches();

  await Promise.all([
    caches.open('v1'),
    caches.open('v2'),
  ]);
  let keys = await caches.keys();
  assert_array_equals(keys, [ 'v1', 'v2' ]);

  await Promise.all([
    caches.delete('v1'),
    caches.delete('v2'),
  ]);
  keys = await caches.keys();
  assert_array_equals(keys, []);
}, 'CacheStorage concurrent open/delete');

promise_test(async () => {
  await cleanCaches();

  await caches.open('v1');
  let keys = await caches.keys();
  assert_array_equals(keys, [ 'v1' ]);

  await caches.delete('v1');
  keys = await caches.keys();
  assert_array_equals(keys, []);
}, 'CacheStorage.prototype.keys');

promise_test(async () => {
  await cleanCaches();

  await caches.open('v1');
  for (const keys of await Promise.all([ caches.keys(), caches.keys() ])) {
    assert_array_equals(keys, [ 'v1' ]);
  }

  await caches.delete('v1');
  for (const keys of await Promise.all([ caches.keys(), caches.keys() ])) {
    assert_array_equals(keys, []);
  }
}, 'CacheStorage.prototype.keys: concurrent');
