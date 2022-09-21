// META: flags=--experimental-curl-fetch
'use strict';

promise_test(async function() {
  const res = await fetch('http://localhost:30122/echo', { method: 'GET' });
  assert_true(res instanceof Response);
  assert_equals(res.status, 200);
  assert_equals(res.headers.get('x-anc-remote-address'), '127.0.0.1');
  assert_equals(res.headers.get('x-anc-remote-port'), '30122');
  assert_equals(res.headers.get('x-anc-remote-family'), 'IPv4');
}, 'x-anc extension headers');
