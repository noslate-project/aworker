// META: same-origin-shared-data=true
'use strict';

promise_test(async () => {
  const storage = new aworker.AsyncLocalStorage();

  await storage.run(1, async () => {
    await caches.delete('v1');
    assert_equals(storage.getStore(), 1);

    const cache = await caches.open('v1');
    assert_equals(storage.getStore(), 1);

    await cache.add('http://localhost:30122/dump');
    assert_equals(storage.getStore(), 1);

    const responses = await cache.matchAll('http://localhost:30122/dump');
    assert_equals(responses.length, 1);
    assert_equals(storage.getStore(), 1);
  });

  assert_equals(storage.getStore(), undefined);
}, 'Cache async local storage propagation');
