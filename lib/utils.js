'use strict';

const { isAbsolute } = load('path');
const { isArrayBufferView, isArrayBuffer, isUint8Array, isTypedArray } = load('types');
const { TextEncoder } = load('encoding');

const lazy = {
  get URL() {
    const { URL } = load('url');
    Object.defineProperty(this, 'URL', {
      value: URL,
    });
    return URL;
  },
};

function requiredArguments(name, length, required) {
  if (length < required) {
    const errMsg = `${name} requires at least ${required} argument${
      required === 1 ? '' : 's'
    }, but only ${length} present`;
    throw new TypeError(errMsg);
  }
}

function getPathFromURLPosix(url) {
  if (url.hostname !== '') {
    throw new TypeError('getPathFromURLPosix: Invalid hostname');
  }
  const pathname = url.pathname;
  for (let n = 0; n < pathname.length; n++) {
    if (pathname[n] === '%') {
      const third = pathname.codePointAt(n + 2) | 0x20;
      if (pathname[n + 1] === '2' && third === 102) {
        throw new TypeError(
          'getPathFromURLPosix: Invalid file path, must not include encoded / characters'
        );
      }
    }
  }
  return decodeURIComponent(pathname);
}

function isURLInstance(fileURLOrPath) {
  return fileURLOrPath != null && fileURLOrPath.href && fileURLOrPath.origin;
}

function fileURLToPath(path) {
  if (typeof path === 'string') {
    path = new lazy.URL(path);
  } else if (!isURLInstance(path)) {
    throw new TypeError('Argument path must be either string or URL');
  }
  if (path.protocol !== 'file:') {
    throw new TypeError('Argument path must be with protocol "file:"');
  }
  return getPathFromURLPosix(path);
}

function normalizeReferrerURL(referrer) {
  if (typeof referrer === 'string' && isAbsolute(referrer)) {
    return (new lazy.URL(referrer, 'file://')).href;
  }
  return (new lazy.URL(referrer)).href;
}

function toArrayBuffer(data) {
  const enc = new TextEncoder();

  let offset = 0;
  let length = 0;
  let buff = data;

  if ((buff instanceof ArrayBuffer)) {
    return { buff, offset, length: buff.byteLength };
  }

  // if string:
  //   String -> Uint8Array
  if (typeof buff === 'string') {
    buff = enc.encode(buff);
  }

  // if TypedArray (Uint8Array / Uint16Array / etc.):
  //   TypedArray -> ArrayBuffer
  if (isArrayBufferView(buff)) {
    offset = buff.byteOffset;
    length = buff.byteLength;
    buff = buff.buffer;
  }

  if (!(buff instanceof ArrayBuffer)) {
    throw new TypeError('`data` only support ArrayBuffer, TypedArray and string.');
  }

  return { buff, offset, length };
}

function bufferLikeToUint8Array(value) {
  const encoder = new TextEncoder();
  if (value == null) {
    throw new TypeError('Value can not be nil');
  }
  if (isUint8Array(value)) {
    return value;
  } else if (isTypedArray(value)) {
    return new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
  } else if (isArrayBuffer(value)) {
    return new Uint8Array(value);
  } else if (typeof value === 'string') {
    return encoder.encode(value);
  }
  throw new TypeError(
    `Value is not buffer like: ${value.constructor.name}`
  );
}

const safeErrorFallback = {
  message: 'Internal Error (Unable to serialize error info)',
  stack: '',
};
function safeError(error) {
  if (error == null) {
    return { ...safeErrorFallback };
  }
  try {
    return {
      message: String(error.message || error),
      stack: String(error.stack),
    };
  } catch {
    return { ...safeErrorFallback };
  }
}

function createDeferred() {
  let resolve;
  let reject;
  const promise = new Promise((_resolve, _reject) => { resolve = _resolve; reject = _reject; });
  return { promise, resolve, reject };
}

const ObjectHasOwnProperty = Object.prototype.hasOwnProperty;
function hasOwn(obj, propKey) {
  return ObjectHasOwnProperty.call(obj, propKey);
}

wrapper.mod = {
  createDeferred,
  requiredArguments,
  fileURLToPath,
  hasOwn,
  normalizeReferrerURL,
  toArrayBuffer,
  bufferLikeToUint8Array,
  safeError,
};
