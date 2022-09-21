'use strict';

promise_test(async () => {
  const mod = await import('../fixtures/esm-dynamic-recursive/a.mjs');
  const res = await mod.foo();
  assert_equals(res, 'bar');
}, 'dynamic import in scripts');
