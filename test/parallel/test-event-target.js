'use strict';

test(() => {
  const target = new EventTarget();
  assert_array_equals(Object.keys(target), []);
  assert_array_equals(Object.getOwnPropertySymbols(target), []);
}, 'EventTarget instance hidden details');

test(() => {
  assert_array_equals(Object.keys(EventTarget), []);
  assert_array_equals(Object.getOwnPropertySymbols(EventTarget), []);
}, 'EventTarget hidden details');
