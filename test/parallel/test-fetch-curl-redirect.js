// META: flags=--experimental-curl-fetch
'use strict';

promise_test(async function() {
  const res = await fetch('http://localhost:30122/redirect', { method: 'GET' });
  assert_true(res instanceof Response);
  assert_equals(res.status, 200);
  assert_equals(res.url, 'http://localhost:30122/dump');
  const body = await res.json();
  assert_equals(body.method, 'GET');
  assert_equals(body.url, '/dump');
}, 'default redirect: follow');

promise_test(async function() {
  const res = await fetch('http://localhost:30122/redirect', { method: 'GET', redirect: 'follow' });
  assert_true(res instanceof Response);
  assert_equals(res.status, 200);
  assert_equals(res.url, 'http://localhost:30122/dump');
  const body = await res.json();
  assert_equals(body.method, 'GET');
  assert_equals(body.url, '/dump');
}, 'redirect: follow');

promise_test(async function() {
  const res = await fetch('http://localhost:30122/redirect', { method: 'GET', redirect: 'manual' });
  assert_true(res instanceof Response);
  assert_equals(res.status, 301);
  assert_equals(res.url, 'http://localhost:30122/redirect');
  assert_equals(res.headers.get('location'), 'http://localhost:30122/dump');
}, 'redirect: manual');
