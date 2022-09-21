// META: location=http://localhost:30122
// META: flags=--experimental-curl-fetch
'use strict';

test(() => {
  const request = new Request('/echo');
  assert_equals(request.url, 'http://localhost:30122/echo');
}, 'full url should be observable on constructed request');

promise_test(async function() {
  const res = await fetch('/dump', { method: 'GET' });
  assert_true(res instanceof Response);
  assert_equals(res.status, 200);
  const data = await res.text();
  const dump = JSON.parse(data);
  assert_equals(dump.method, 'GET');
  assert_equals(dump.body, '');
}, 'fetch a relative url return a response with body');
