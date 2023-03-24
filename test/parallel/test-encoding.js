'use strict';

test(() => {
  const encoder = new TextEncoder();
  const decoder = new TextDecoder();

  const expected = '👩‍👩‍👦‍👦';
  const buf = encoder.encode(expected);
  const partialBuf = buf.subarray(0, 1);

  const partial = decoder.decode(partialBuf);
  // https://encoding.spec.whatwg.org/#concept-encoding-process
  // "replacement"
  assert_equals(partial, '\uFFFD');

  const actual = decoder.decode(buf);
  assert_equals(actual, expected);
}, 'text decoder decode partial replacement');

test(() => {
  const encoder = new TextEncoder();
  const decoder = new TextDecoder('utf8', { fatal: true });

  const buf = encoder.encode('👩‍👩‍👦‍👦');
  const partialBuf = buf.subarray(0, 1);

  assert_throws_js(TypeError, () => decoder.decode(partialBuf));
}, 'text decoder decode partial fatal');
