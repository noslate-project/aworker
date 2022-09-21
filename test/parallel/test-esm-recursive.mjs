// META: flags=--module

import { foo } from '../fixtures/esm-static-recursive/a.mjs';

test(() => {
  assert_equals(foo, 'bar');
}, 'static recursive dependencies');

promise_test(async () => {
  const mod = await import('../fixtures/esm-dynamic-recursive/a.mjs');
  const res = await mod.foo();
  assert_equals(res, 'bar');
}, 'dynamic recursive dependencies');
