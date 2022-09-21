'use strict';

const binding = loadBinding('bytes');

function atob(encodedData) {
  return binding.atob(String(encodedData));
}

function btoa(stringToEncode) {
  return binding.btoa(String(stringToEncode));
}

function validateOffsetAndLength(buff, offset, length) {
  if (typeof offset !== 'number' || isNaN(offset)) {
    throw new TypeError('offset should be a number.');
  }

  if (offset < 0 || offset >= Math.max(buff.byteLength, 1)) {
    throw new RangeError(`offset should in range of (0, ${Math.max(buff.byteLength - 1, 0)})`);
  }

  if (typeof length !== 'number' || isNaN(length)) {
    throw new TypeError('length should be a number.');
  }

  if (length < 0 || length > buff.byteLength - offset) {
    throw new RangeError(`length should in range of (0, ${buff.byteLength - offset})`);
  }
}

function bin2x(func, buff, offset, length) {
  if (!(buff instanceof ArrayBuffer)) {
    throw new TypeError('buff is not an ArrayBuffer.');
  }

  if (length === undefined) {
    length = buff.byteLength;
  }

  validateOffsetAndLength(buff, offset, length);

  if (length === 0) {
    return new ArrayBuffer(0);
  }

  return func(buff, offset, length);
}

function bufferEncodeHex(buff, offset = 0, length = undefined) {
  return bin2x(binding.bufferEncodeHex, buff, offset, length);
}

function bufferEncodeBase64(buff, offset = 0, length = undefined) {
  return bin2x(binding.bufferEncodeBase64, buff, offset, length);
}

wrapper.mod = {
  atob,
  btoa,
  bufferEncodeHex,
  bufferEncodeBase64,
};
