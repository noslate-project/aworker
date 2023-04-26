// META: flags=--mode=seed-userland --no-experimental-curl-fetch
'use strict';

const installedFuture = new Promise(resolve => {
  addEventListener('install', () => {
    resolve();
  });
});

promise_test(async function() {
  await installedFuture;
  const res = await fetch('http://localhost:30122/echo', { method: 'GET' });
  assert_true(res instanceof Response);
  assert_equals(res.status, 200);
}, 'fetch return a response');
