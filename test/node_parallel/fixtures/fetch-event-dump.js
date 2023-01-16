'use strict';

const { assert } = load('assert');

const kBaggage = Symbol.for('aworker.fetch.baggage');
addEventListener('fetch', event => {
  assert(event.request);
  /** @type {Request} */
  const request = event.request;
  assert(request instanceof Request);
  assert(!request.bodyUsed);
  const p = request.text()
    .then(text => {
      const baggage = Array.from(request[kBaggage]?.entries() ?? []);

      return new Response(JSON.stringify({
        url: request.url,
        method: request.method,
        headers: Array.from(request.headers.entries()),
        baggage,
        text,
      }), {
        headers: {
          'x-worker': 'my-worker-header',
        },
      });
    });

  event.respondWith(p);
});
