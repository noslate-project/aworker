// META: flags=--experimental-curl-fetch
'use strict';

promise_test(async function() {
  const storage = new aworker.AsyncLocalStorage();

  await storage.run(1, async () => {
    const res = await fetch('http://localhost:30122/echo', { method: 'GET' });
    assert_true(res instanceof Response);
    assert_equals(res.status, 200);
    assert_equals(storage.getStore(), 1);
  });
}, 'fetch fulfilled with async local storage propagation');

promise_test(async function() {
  const storage = new aworker.AsyncLocalStorage();

  await storage.run(1, async () => {
    const res = await fetch('http://localhost:30122/dump', { method: 'GET' });
    assert_true(res instanceof Response);
    assert_equals(res.status, 200);
    assert_equals(storage.getStore(), 1);

    const reader = res.body.getReader();
    while (true) {
      const { done } = await reader.read();
      assert_equals(storage.getStore(), 1);
      if (done) {
        break;
      }
    }
  });
}, 'fetch body reader fulfilled with async local storage propagation');
