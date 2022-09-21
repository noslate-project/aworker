// META: flags=--experimental-curl-fetch
'use strict';

promise_test(async function() {
  const res = await fetch('http://localhost:30122/dump', { method: 'POST' });
  assert_true(res instanceof Response);
  assert_equals(res.status, 200);
  const data = await res.text();
  const dump = JSON.parse(data);
  assert_equals(dump.method, 'POST');
  assert_equals(dump.body, '');
}, 'fetch with method POST and empty body');

promise_test(async function() {
  const res = await fetch('http://localhost:30122/dump', { method: 'POST', body: 'foobar' });
  assert_true(res instanceof Response);
  assert_equals(res.status, 200);
  const data = await res.text();
  const dump = JSON.parse(data);
  console.log(dump);
  assert_equals(dump.method, 'POST');
  const contentType = dump.headers['content-type'];
  assert_equals(contentType, 'text/plain;charset=UTF-8');
  assert_equals(dump.body, 'foobar');
}, 'fetch with method POST and a text body');

promise_test(async function() {
  const body = new URLSearchParams([[ 'foo', 'bar=' ], [ 'qux', 'quz&' ]]);
  const res = await fetch('http://localhost:30122/dump', { method: 'POST', body });
  assert_true(res instanceof Response);
  assert_equals(res.status, 200);
  const data = await res.text();
  const dump = JSON.parse(data);
  assert_equals(dump.method, 'POST');
  const contentType = dump.headers['content-type'];
  assert_equals(contentType, 'application/x-www-form-urlencoded;charset=UTF-8');
  assert_equals(dump.body, 'foo=bar%3D&qux=quz%26');
}, 'fetch with method POST and a URLSearchParams');

// TODO(chengzhong.wcz): should infer content-length from body type.
promise_test(async function() {
  const body = new URLSearchParams([[ 'foo', 'bar=' ], [ 'qux', 'quz&' ]]);
  const bodyBuffer = new TextEncoder().encode(body.toString());
  const headers = new Headers({
    'content-length': bodyBuffer.byteLength,
    'content-type': 'application/x-www-form-urlencoded',
  });

  const res = await fetch('http://localhost:30122/dump', { method: 'POST', headers, body: bodyBuffer });
  assert_true(res instanceof Response);
  assert_equals(res.status, 200);
  const data = await res.text();
  const dump = JSON.parse(data);
  assert_equals(dump.method, 'POST');
  assert_equals(dump.headers['content-length'], `${bodyBuffer.byteLength}`);
  assert_equals(dump.headers['content-type'], 'application/x-www-form-urlencoded');
  assert_equals(dump.body, 'foo=bar%3D&qux=quz%26');
}, 'fetch with method POST and content-length');

promise_test(async function() {
  const body = new FormData();
  body.append('foo', 'bar');

  const res = await fetch('http://localhost:30122/dump', { method: 'POST', body });
  assert_true(res instanceof Response);
  assert_equals(res.status, 200);
  const data = await res.text();
  const dump = JSON.parse(data);
  assert_equals(dump.method, 'POST');
  const contentType = dump.headers['content-type'];
  assert_equals(typeof contentType, 'string');
  const boundary = contentType.split(';').find(it => {
    it = it.trim();
    return it.startsWith('boundary=');
  }).substring('broundary='.length);
  assert_true(boundary != null);
  assert_equals(dump.body, `--${boundary}\r\nContent-Disposition: form-data; name=\"foo\"\r\n\r\nbar\r\n--${boundary}--`);
}, 'fetch with method POST and a FormData');
