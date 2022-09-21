// META: same-origin-shared-data=true
'use strict';

const url = 'http://localhost:30122/slow-emit-exclusive';
const statusEchoUrl = 'http://localhost:30122/status-echo';
const statsUrl = 'http://localhost:30122/stats';

promise_test(async () => {
  await caches.delete('v1');
  const cache = await caches.open('v1');

  await Promise.all([
    cache.add(url),
    cache.add(url),
  ]);

  const responses = await cache.matchAll(url);
  assert_equals(responses.length, 1, 'cache matched');

  {
    const resp = await fetch(statsUrl);
    const json = await resp.json();
    assert_equals(json.requestCount['/slow-emit-exclusive'], 1);
  }
}, 'Cache.prototype.add: concurrent');

promise_test(async () => {
  await caches.delete('v1');
  const cache = await caches.open('v1');

  await Promise.all([
    cache.add(url),
    cache.add(url),
  ]);
  await Promise.all([
    cache.add(url),
    cache.add(url),
  ]);

  {
    const responses = await cache.matchAll(url);
    assert_equals(responses.length, 1, 'cache matched');
  }

  await cache.delete(url);

  {
    const responses = await cache.matchAll(url);
    assert_equals(responses.length, 0, 'cache matched');
  }

  await Promise.all([
    cache.add(url),
    cache.add(url),
  ]);

  {
    const responses = await cache.matchAll(url);
    assert_equals(responses.length, 1, 'cache matched');
  }

  {
    const resp = await fetch(statsUrl);
    const json = await resp.json();
    assert_equals(json.requestCount['/slow-emit-exclusive'], 2);
  }
}, 'Cache.prototype.add: sequential');

promise_test(async () => {
  await caches.delete('v1');
  const cache = await caches.open('v1');

  const result = await Promise.allSettled([
    cache.add(new Request(statusEchoUrl, {
      headers: {
        'x-status-echo': '400',
      },
    })),
    cache.add(new Request(statusEchoUrl, {
      headers: {
        'x-status-echo': '200',
      },
    })),
  ]);
  assert_equals(result[0].status, 'rejected');
  assert_equals(result[1].status, 'fulfilled');

  const responses = await cache.matchAll(statusEchoUrl);
  assert_equals(responses.length, 1, 'cache matched');

  {
    const resp = await fetch(statsUrl);
    const json = await resp.json();
    assert_equals(json.requestCount['/status-echo'], 2);
  }
}, 'Cache.prototype.add: concurrent failed once');
