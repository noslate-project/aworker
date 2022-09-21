// META: flags=--experimental-curl-fetch
'use strict';

promise_test(async () => {
  const forbiddenRequestHeaders = [
    'accept-charset',
    'accept-encoding',
    'access-control-request-headers',
    'access-control-request-method',
    // SKIP: 'connection',
    // SKIP: 'content-length',
    'cookie',
    'cookie2',
    'date',
    'dnt',
    // SKIP: 'expect',
    'host',
    // SKIP: 'keep-alive',
    'origin',
    'referer',
    // SKIP: 'te',
    // SKIP: 'trailer',
    // SKIP: 'transfer-encoding',
    // SKIP: 'upgrade',
    'via',
    // prox- prefixed
    'proxy-foobar',
    // sec- prefixed
    'sec-foobar',
  ];
  const headers = Object.fromEntries(forbiddenRequestHeaders.map(it => [ it, 'foobar' ]));
  const request = new Request('http://localhost:30122/dump', { method: 'GET', headers });

  const res = await fetch(request);
  assert_true(res instanceof Response);
  assert_equals(res.status, 200);
  assert_equals(res.headers.get('x-server'), 'aworker-resource-server');

  const data = await res.text();
  const dump = JSON.parse(data);
  assert_equals(dump.method, 'GET');

  for (const [ key, val ] of Object.entries(headers)) {
    assert_equals(dump.headers[key], val, `validate request header '${key}'`);
  }
}, 'fetch with safe-mode forbidden request headers');

promise_test(async () => {
  const request = new Request('http://localhost:30122/dump', { method: 'GET' });

  const res = await fetch(request);
  assert_true(res instanceof Response);
  assert_equals(res.status, 200);
  assert_equals(res.headers.get('x-server'), 'aworker-resource-server');
  assert_equals(res.headers.get('set-cookie'), 'foobar');
}, 'fetch with safe-mode forbidden response headers');
