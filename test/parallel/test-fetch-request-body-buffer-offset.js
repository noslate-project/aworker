'use strict';

promise_test(async function() {
  const ab = new ArrayBuffer(10);
  const u8 = new Uint8Array(ab);
  u8[0] = 1;
  u8[5] = 2;
  const request = new Request('http://example.com', {
    method: 'LADIDA',
    body: new Uint8Array(ab, 5, 5),
  });
  const body = new Uint8Array(await request.arrayBuffer());
  assert_equals(body[0], 2);
  assert_equals(body.byteLength, 5);

  // Body should not be a reference.
  body[0] = 3;
  assert_equals(u8[5], 2);
}, 'create request with Uint8Array and buffer offset');

