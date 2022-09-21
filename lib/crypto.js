'use strict';

const {
  Hash: _Hash,
  Hmac: _Hmac,
  randomBytes: _randomBytes,
} = loadBinding('crypto');
const { bufferEncodeHex, bufferEncodeBase64 } = load('bytes');
const { TextDecoder } = load('encoding');
const { toArrayBuffer } = load('utils');

function randomBytes(size) {
  if (typeof size !== 'number') {
    throw new TypeError('size should be a number');
  }

  if (size < 0) {
    throw new RangeError('size should be a positive number');
  }

  return new Uint8Array(_randomBytes(size));
}

const dec = new TextDecoder();
class FinalResult {
  #buffer;

  constructor(buffer) {
    this.#buffer = buffer;
  }

  digest(type) {
    if (!type) {
      type = 'typedArray';
    }

    switch (type) {
      case 'utf8':
      case 'utf-8': {
        return dec.decode(this.#buffer);
      }

      case 'hex': {
        return dec.decode(bufferEncodeHex(this.#buffer));
      }

      case 'base64': {
        return dec.decode(bufferEncodeBase64(this.#buffer));
      }

      case 'typedArray':
      case 'binary': {
        return new Uint8Array(this.#buffer.slice(0));
      }

      default: {
        throw new TypeError(`Unsupported digest type ${type}.`);
      }
    }
  }
}

class Hash {
  #handle;
  constructor(algorithm, key, keyOffset, keyLength) {
    this.#handle = new _Hash(algorithm, key, keyOffset, keyLength);
  }

  update(data) {
    const { buff, offset, length } = toArrayBuffer(data);
    return this.#handle.update(buff, offset, length);
  }

  final() {
    return new FinalResult(this.#handle.digest());
  }
}

class Hmac {
  #handle;
  constructor(algorithm, key) {
    const { buff: keyBuff, offset: keyOffset, length: keyLength } = toArrayBuffer(key);

    this.#handle = new _Hmac(algorithm, keyBuff, keyOffset, keyLength);
  }

  update(data) {
    const { buff, offset, length } = toArrayBuffer(data);
    return this.#handle.update(buff, offset, length);
  }

  final() {
    return new FinalResult(this.#handle.digest());
  }
}

wrapper.mod = {
  Hash,
  Hmac,
  randomBytes,
};
