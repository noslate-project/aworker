// META: flags=--experimental-curl-fetch
'use strict';

const statsUrl = 'http://localhost:30122/stats';

promise_test(async function(t) {
  const ac = new AbortController();
  ac.abort();
  await promise_rejects_dom(t, DOMException.ABORT_ERR, fetch('http://localhost:30122/echo', { method: 'GET', signal: ac.signal }));

  {
    const resp = await fetch(statsUrl);
    const json = await resp.json();
    assert_equals(json.requestCount['/echo'], undefined);
  }
}, 'should not send request if the signal is been aborted already');

promise_test(async function(t) {
  const ac = new AbortController();
  const future = fetch('http://localhost:30122/black-hole', { method: 'GET', signal: ac.signal });
  await new Promise(resolve => setTimeout(resolve, 100));
  ac.abort();

  await promise_rejects_dom(t, DOMException.ABORT_ERR, future);

  {
    const resp = await fetch(statsUrl);
    const json = await resp.json();
    assert_equals(json.requestCount['/black-hole'], 1);
  }
}, 'fetch abort GET header');

promise_test(async function(t) {
  const ac = new AbortController();
  let bodyCancelReason;
  const body = new ReadableStream({
    pull(controller) {
      return new Promise(resolve => {
        setTimeout(() => {
          // enqueue when the body has not been cancelled yet.
          if (bodyCancelReason == null) {
            controller.enqueue('foo');
          }
          resolve();
        }, 10);
      });
    },
    cancel(reason) {
      bodyCancelReason = reason;
    },
  });
  const future = fetch('http://localhost:30122/black-hole', { method: 'POST', signal: ac.signal, body });
  await new Promise(resolve => setTimeout(resolve, 100));
  ac.abort();

  await promise_rejects_dom(t, DOMException.ABORT_ERR, future);

  assert_throws_dom(DOMException.ABORT_ERR, () => { throw bodyCancelReason; });

  {
    const resp = await fetch(statsUrl);
    const json = await resp.json();
    assert_equals(json.requestCount['/black-hole'], 1);
  }
}, 'fetch abort POST header');

promise_test(async function(t) {
  const ac = new AbortController();
  const response = await fetch('http://localhost:30122/slow-emit', { method: 'GET', signal: ac.signal });
  assert_equals(response.status, 200);
  const reader = response.body.getReader();
  for (let idx = 0; idx++; idx < 3) {
    const { done } = await reader.read();
    assert_equals(done, false);
  }
  ac.abort();

  await promise_rejects_dom(t, DOMException.ABORT_ERR, reader.read());
}, 'fetch abort body');
