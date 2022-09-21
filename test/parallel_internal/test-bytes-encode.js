'use strict';

const { bufferEncodeHex, bufferEncodeBase64 } = load('bytes');
const decoder = new TextDecoder();
const encoder = new TextEncoder();

test(() => {
  const uarray = new Uint8Array([ 1, 2, 3, 4, 5, 6, 7 ]);
  assert_equals(decoder.decode(bufferEncodeBase64(uarray.buffer)), 'AQIDBAUGBw==');
}, 'encode binary data with base64');

test(() => {
  const uarray = new Uint8Array([ 1, 2, 3, 4, 5, 6, 7 ]);
  assert_equals(decoder.decode(bufferEncodeHex(uarray.buffer)), '01020304050607');
}, 'encode binary data with hex');

test(() => {
  const uarray = encoder.encode('Hello world');
  assert_equals(decoder.decode(bufferEncodeBase64(uarray.buffer)), 'SGVsbG8gd29ybGQ=');
}, 'encode string buffer with base64');

test(() => {
  const uarray = encoder.encode('Hello world');
  assert_equals(decoder.decode(bufferEncodeBase64(uarray.buffer)), 'SGVsbG8gd29ybGQ=');
}, 'encode string buffer with hex');

test(() => {
  const uarray = encoder.encode('Hello world');
  assert_equals(decoder.decode(bufferEncodeBase64(uarray.buffer, 1, 5)), 'ZWxsbyA=');

  let e;
  try {
    bufferEncodeBase64(uarray.buffer, 1, 15);
  } catch (_e) {
    e = _e;
  }
  assert_equals(e && e.message, 'length should in range of (0, 10)');

  assert_equals(decoder.decode(bufferEncodeBase64(uarray.buffer, 0, 0)), '');
}, 'encode string buffer with base64 with offset / length');


test(() => {
  const uarray = encoder.encode('Hello world');
  assert_equals(decoder.decode(bufferEncodeHex(uarray.buffer, 1, 5)), '656c6c6f20');

  let e;
  try {
    bufferEncodeHex(uarray.buffer, -1, 3);
  } catch (_e) {
    e = _e;
  }
  assert_equals(e && e.message, 'offset should in range of (0, 10)');

  assert_equals(decoder.decode(bufferEncodeHex(uarray.buffer, 0, 0)), '');
}, 'encode string buffer with hex with offset / length');


test(() => {
  const uarray = new Uint8Array(0);
  assert_equals(decoder.decode(bufferEncodeBase64(uarray.buffer)), '');
  assert_equals(decoder.decode(bufferEncodeHex(uarray.buffer)), '');
}, 'encode with empty ArrayBuffer');
