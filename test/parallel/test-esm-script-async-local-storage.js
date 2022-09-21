'use strict';

promise_test(async () => {
  const storage = new aworker.AsyncLocalStorage();

  await storage.run(1, async () => {
    const mod = await import('../fixtures/esm-dynamic-recursive/a.mjs');
    const res = await mod.foo();
    assert_equals(res, 'bar');

    assert_equals(storage.getStore(), 1);
  });

  assert_equals(storage.getStore(), undefined);
}, 'dynamic import should propagate async local storage');
