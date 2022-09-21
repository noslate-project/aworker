// META: flags=--experimental-curl-fetch
'use strict';

promise_test(async function() {
  let streamedBody = '';
  const readable = new ReadableStream({
    start(controller) {
      let times = 0;
      const interval = setInterval(() => {
        const string = 'foobar';
        controller.enqueue(string);
        streamedBody += string;
        times++;

        if (times >= 100) {
          clearInterval(interval);
          controller.close();
        }
      }, 1);
    },
  });
  const res = await fetch('http://localhost:30122/dump', { method: 'POST', body: readable });
  assert_true(res instanceof Response);
  assert_equals(res.status, 200);
  const data = await res.text();
  const dump = JSON.parse(data);
  assert_equals(dump.method, 'POST');
  assert_equals(dump.body, streamedBody);
}, 'fetch with method POST and a streaming body');

promise_test(async function(t) {
  const body = new ReadableStream({
    start(controller) {
      setTimeout(() => {
        controller.error(new Error('foobar'));
      }, 100);
    },
  });
  promise_rejects_js(t, TypeError, fetch('http://localhost:30122/dump', { method: 'POST', body }));
}, 'abort request when streaming body aborts');
