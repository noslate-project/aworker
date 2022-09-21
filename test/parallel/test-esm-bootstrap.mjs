// META: flags=--module

import { count, addCount } from '../fixtures/foo.mjs';
import * as foo from '../fixtures/foo.mjs';

test(() => {
  assert_true(true, 'OK!');
  assert_equals(count, 0);
  addCount();
  assert_equals(foo.count, 1);
  assert_equals(count, 1);
}, 'bootstrap');

promise_test(async () => {
  assert_true(true, 'OK!');
  const mod = await import('../fixtures/foo.mjs');
  assert_equals(mod.count, count);
  assert_equals(mod, foo);
  mod.addCount();
  assert_equals(mod.count, foo.count);
}, 'dynamic import');

test(() => {
  assert_equals(typeof import.meta.url, 'string');
  const url = new URL(import.meta.url);
  assert_equals(url.protocol, 'file:');
  assert_regexp_match(url.pathname, /\/test-esm-bootstrap.mjs/);
  assert_equals(url.pathname, location.pathname);
}, 'import.meta');
