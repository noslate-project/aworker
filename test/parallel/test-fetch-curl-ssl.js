// META: flags=--experimental-curl-fetch
'use strict';

promise_test(async function() {
  const res = await fetch('https://www.alibaba.com', { method: 'HEAD' });
  assert_true(res instanceof Response);
  assert_equals(res.status, 200);
}, 'fetch return a response');
