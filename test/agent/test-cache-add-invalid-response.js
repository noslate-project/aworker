// META: same-origin-shared-data=true
'use strict';

const invalidStatuses = [ 206, 301, 302, 400, 500 ];

const kStatusEchoUrl = 'http://localhost:30122/status-echo';
promise_test(async () => {
  await caches.delete('v1');
  const cache = await caches.open('v1');
  for (const status of invalidStatuses) {
    const request = new Request(kStatusEchoUrl, {
      headers: {
        'x-status-echo': `${status}`,
      },
    });
    try {
      await cache.add(request);
    } catch (e) {
      assert_true(e instanceof TypeError);
      assert_true(e.response instanceof Response);
      assert_equals(e.response.status, status);
    }

    const matched = await cache.matchAll(kStatusEchoUrl);
    assert_equals(matched.length, 0);
  }
}, 'Cache.prototype.add: invalid statuses');


const kDumpHost = 'http://localhost:30122/dump';
promise_test(async () => {
  await caches.delete('v1');
  const cache = await caches.open('v1');

  const preset = [
    '*',
    'it,*',
  ];
  for (const item of preset) {
    const request = new Request(kDumpHost, {
      headers: {
        'x-set-header': `Vary=${item}`,
      },
    });
    try {
      await cache.add(request);
    } catch (e) {
      assert_true(e instanceof TypeError);
      assert_true(e.response instanceof Response);
      assert_equals(e.response.status, 200);
      assert_equals(e.response.headers.get('vary'), item);
    }

    const matched = await cache.matchAll(kDumpHost);
    assert_equals(matched.length, 0);
  }

}, 'Cache.prototype.add: wildcard vary');


const kInvalidHost = 'http://unreachable-host';
promise_test(async () => {
  await caches.delete('v1');
  const cache = await caches.open('v1');

  const request = new Request(kInvalidHost);
  try {
    await cache.add(request);
  } catch (e) {
    // TODO: fetch error?
    assert_true(e instanceof Error);
  }

  const matched = await cache.matchAll(kInvalidHost);
  assert_equals(matched.length, 0);
}, 'Cache.prototype.add: unreachable host');
