// META: same-origin-shared-data=true
'use strict';

async function cleanCaches() {
  const keys = await caches.keys();
  await Promise.all(keys.map(it => caches.delete(it)));
}

promise_test(async () => {
  await cleanCaches();
  const cache = await caches.open('v1');
  const keys = await cache.keys();
  assert_array_equals(keys, []);
}, 'Cache.prototype.keys: empty storage');

promise_test(async () => {
  await cleanCaches();
  const cache = await caches.open('v1');
  {
    const keys = await cache.keys();
    assert_array_equals(keys, []);
  }

  await cache.put('http://foobar.com/', new Response('foobar'));
  {
    const keys = await cache.keys();
    assert_equals(keys.length, 1);
    assert_equals(keys[0].url, 'http://foobar.com/');
  }

  await cache.delete('http://foobar.com/');
  {
    const keys = await cache.keys();
    assert_array_equals(keys, []);
  }

  await cache.put('http://foobar.com/', new Response('foobar'));
  {
    const keys = await cache.keys();
    assert_equals(keys.length, 1);
    assert_equals(keys[0].url, 'http://foobar.com/');
  }
}, 'Cache.prototype.keys: list put item');
