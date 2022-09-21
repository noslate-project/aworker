// META: flags=--no-experimental-curl-fetch
'use strict';

promise_test(async function() {
  const res = await fetch('http://localhost:30122/echo', { method: 'GET' });
  assert_true(res instanceof Response);
  assert_equals(res.status, 200);
}, 'fetch return a response');

promise_test(async function() {
  const res = await fetch('http://localhost:30122/dump', { method: 'GET' });
  assert_true(res instanceof Response);
  assert_equals(res.status, 200);
  const data = await res.text();
  const dump = JSON.parse(data);
  assert_equals(dump.method, 'GET');
  assert_equals(dump.body, '');
}, 'fetch return a response with body');

promise_test(async function() {
  const res = await fetch('http://localhost:30122/dump', { method: 'GET', headers: { 'x-foo': 'bar', 'x-qux': 'quz' } });
  assert_true(res instanceof Response);
  assert_equals(res.status, 200);
  assert_equals(res.headers.get('x-server'), 'aworker-resource-server');

  const data = await res.text();
  const dump = JSON.parse(data);
  assert_equals(dump.method, 'GET');
  assert_equals(dump.headers['x-foo'], 'bar');
  assert_equals(dump.headers['x-qux'], 'quz');
}, 'fetch with method GET and headers return a response with body');

promise_test(async t => {
  promise_rejects_js(t, Error, fetch('http://unreachable-host/echo', { method: 'GET' }));

  try {
    await fetch('http://unreachable-host/echo', { method: 'GET' });
    assert_unreached('unreachable');
  } catch (e) {
    assert_regexp_match(String(e), /Alice request failed/);
    assert_equals(e.code, 1);
    assert_regexp_match(e.peerMessage, /getaddrinfo (ENOTFOUND|EAI_AGAIN) unreachable-host/);
  }
}, 'fetch should throw network error');
