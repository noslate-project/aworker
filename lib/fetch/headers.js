'use strict';

const { DomIterableMixin } = load('dom/iterable');
const { requiredArguments } = load('utils');

// From node-fetch
// Copyright (c) 2016 David Frank. MIT License.
const invalidTokenRegex = /[^\^_`a-zA-Z\-0-9!#$%&'*+.|~]/;
const invalidHeaderCharRegex = /[^\t\x20-\x7e\x80-\xff]/;

const headersData = Symbol('headers data');
const headersGuard = Symbol('headers guard');

// https://fetch.spec.whatwg.org/#http-header-layer-division
const forbiddenRequestHeaders = [
  'accept-charset',
  'accept-encoding',
  'access-control-request-headers',
  'access-control-request-method',
  'connection',
  'content-length',
  'cookie',
  'cookie2',
  'date',
  'dnt',
  'expect',
  'host',
  'keep-alive',
  'origin',
  'referer',
  'te',
  'trailer',
  'transfer-encoding',
  'upgrade',
  'via',
];

const forbiddenRequestHeaderPrefixes = [
  'proxy-',
  'sec-',
];

const noCorsSafeListedRequestHeaders = [
  'accept',
  'accept-language',
  'content-language',
  'content-type',
];

const forbiddenResponseHeaders = [
  'set-cookie',
  'set-cookie2',
];

const privilegedNoCorsRequestHeaderName = [
  'range',
];

const corsUnsafeHeaderBytes = [ 0x22, 0x28, 0x29, 0x3a, 0x3c, 0x3e, 0x3f, 0x40, 0x5b, 0x5c, 0x5d, 0x7b, 0x7d, 0x7f ];

function isForbiddenRequestHeaderName(name) {
  if (forbiddenRequestHeaders.includes(name)) {
    return true;
  }
  for (const prefix of forbiddenRequestHeaderPrefixes) {
    if (name.startsWith(prefix)) {
      return true;
    }
  }
  return false;
}

function isForbiddenResponseHeaderName(name) {
  if (forbiddenResponseHeaders.includes(name)) {
    return true;
  }
  return false;
}

/**
 *
 * @param {string} value -
 */
function isCorsUnsafeRequestHeaderByte(value) {
  for (let i = 0; i < value.length; i++) {
    const charCode = value.charCodeAt(i);
    if (charCode < 0x20 && charCode !== 0x09) {
      return true;
    }
    if (corsUnsafeHeaderBytes.includes(charCode)) {
      return true;
    }
  }
  return false;
}

const languageSafeChars = [ 0x20, 0x2a, 0x2c, 0x2d, 0x2e, 0x3b, 0x3d ];

/**
 *
 * @param {string} name -
 * @param {string} value -
 */
function isNoCorsSafeListedRequestHeader(name, value) {
  if (noCorsSafeListedRequestHeaders.includes(name)) {
    return true;
  }
  if (value === undefined) {
    return false;
  }
  switch (name) {
    case 'accept': {
      if (isCorsUnsafeRequestHeaderByte(value)) {
        return false;
      }
      break;
    }
    case 'accept-language':
    case 'content-language': {
      // If value contains a byte that is not in the range 0x30 (0) to 0x39 (9),
      // inclusive, is not in the range 0x41 (A) to 0x5A (Z), inclusive, is not in
      // the range 0x61 (a) to 0x7A (z), inclusive, and is not 0x20 (SP), 0x2A (*),
      // 0x2C (,), 0x2D (-), 0x2E (.), 0x3B (;), or 0x3D (=), then return false.
      for (let idx = 0; idx < value.length; idx++) {
        const charCode = value.charCodeAt(idx);
        // eslint-disable-next-line yoda
        if (0x30 <= charCode && charCode <= 0x39) {
          continue;
        }
        // eslint-disable-next-line yoda
        if (0x41 <= charCode && charCode <= 0x5a) {
          continue;
        }
        // eslint-disable-next-line yoda
        if (0x61 <= charCode && charCode <= 0x7a) {
          continue;
        }
        if (languageSafeChars.includes(charCode)) {
          continue;
        }
        return false;
      }
      // TODO: content-type
      break;
    }
    default:
  }
  return true;
}

// node-fetch did not implement this but it is in the spec
function normalizeParams(name, value) {
  name = String(name).toLowerCase();
  value = String(value).trim();
  return [ name, value ];
}

// The following name/value validations are copied from
// https://github.com/bitinn/node-fetch/blob/master/src/headers.js
// Copyright (c) 2016 David Frank. MIT License.
function validateName(name) {
  if (invalidTokenRegex.test(name) || name === '') {
    throw new TypeError(`${name} is not a legal HTTP header name`);
  }
}

function validateValue(value) {
  if (invalidHeaderCharRegex.test(value)) {
    throw new TypeError(`${value} is not a legal HTTP header value`);
  }
}

/**
 * Appends a key and value to the header list.
 *
 * The spec indicates that when a key already exists, the append adds the new
 * value onto the end of the existing value.  The behavior of this though
 * varies when the key is `set-cookie`.  In this case, if the key of the cookie
 * already exists, the value is replaced, but if the key of the cookie does not
 * exist, and additional `set-cookie` header is added.
 *
 * The browser specification of `Headers` is written for clients, and not
 * servers, and Aworker is a server runtime, meaning that it needs to follow the patterns
 * expected for servers, of which a `set-cookie` header is expected for each
 * unique cookie key, but duplicate cookie keys should not exist.
 *
 * @param {[string, string][]} data data
 * @param {string} key key
 * @param {unknown} value value
 */
function dataAppend(data, key, value) {
  for (let i = 0; i < data.length; i++) {
    const { 0: dataKey, 1: dataValue } = data[i];
    if (key === 'set-cookie' && dataKey === 'set-cookie') {
      const { 0: dataCookieKey } = dataValue.split('=');
      const { 0: cookieKey } = value.split('=');
      if (dataCookieKey === cookieKey) {
        data[i][1] = value;
        return;
      }
    } else {
      if (dataKey === key) {
        data[i][1] += `, ${value}`;
        return;
      }
    }
  }
  data.push([ key, value ]);
}

/**
 * Gets a value of a key in the headers list.
 *
 * This varies slightly from spec behaviour in that when the key is `set-cookie`
 * the value returned will look like a concatenated value, when in fact, if the
 * headers were iterated over, each individual `set-cookie` value is a unique
 * entry in the headers list.
 *
 * @param {[string, string][]} data data
 * @param {string} key key
 */
function dataGet(data, key) {
  const setCookieValues = [];
  for (const [ dataKey, value ] of data) {
    if (dataKey === key) {
      if (key === 'set-cookie') {
        setCookieValues.push(value);
      } else {
        return value;
      }
    }
  }
  if (setCookieValues.length) {
    return setCookieValues.join(', ');
  }
  return undefined;
}

/**
 * Sets a value of a key in the headers list.
 *
 * The spec indicates that the value should be replaced if the key already
 * exists.  The behavior here varies, where if the key is `set-cookie` the key
 * of the cookie is inspected, and if the key of the cookie already exists,
 * then the value is replaced.  If the key of the cookie is not found, then
 * the value of the `set-cookie` is added to the list of headers.
 *
 * The browser specification of `Headers` is written for clients, and not
 * servers, and Aworker is a server runtime, meaning that it needs to follow the patterns
 * expected for servers, of which a `set-cookie` header is expected for each
 * unique cookie key, but duplicate cookie keys should not exist.
 *
 * @param {[string, string][]} data data
 * @param {string} key key
 * @param {unknown} value value
 */
function dataSet(data, key, value) {
  for (let i = 0; i < data.length; i++) {
    const { 0: dataKey, 1: dataValue } = data[i];
    if (dataKey === key) {
      // there could be multiple set-cookie headers, but all others are unique
      if (key === 'set-cookie') {
        const { 0: dataCookieKey } = dataValue.split('=');
        const { 0: cookieKey } = value.split('=');
        if (cookieKey === dataCookieKey) {
          data[i][1] = value;
          return;
        }
      } else {
        data[i][1] = value;
        return;
      }
    }
  }
  data.push([ key, value ]);
}

function dataDelete(data, key) {
  let i = 0;
  while (i < data.length) {
    const { 0: dataKey } = data[i];
    if (dataKey === key) {
      data.splice(i, 1);
    } else {
      i++;
    }
  }
}

function dataHas(data, key) {
  for (const [ dataKey ] of data) {
    if (dataKey === key) {
      return true;
    }
  }
  return false;
}

function dataRemovePrivilegedNoCorsRequestHeaders(data) {
  let i = 0;
  while (i < data.length) {
    const [ dataKey ] = data[i];
    if (privilegedNoCorsRequestHeaderName.includes(dataKey)) {
      data.splice(i, 1);
    } else {
      i++;
    }
  }
}

// ref: https://fetch.spec.whatwg.org/#dom-headers
class HeadersBase {
  constructor(init) {
    if (init === null) {
      throw new TypeError(
        "Failed to construct 'Headers'; The provided value was not valid"
      );
    }

    if (isHeaders(init)) {
      this[headersData] = [ ...init ];
    } else {
      this[headersData] = [];
      if (Array.isArray(init)) {
        for (const tuple of init) {
          // If header does not contain exactly two items,
          // then throw a TypeError.
          // ref: https://fetch.spec.whatwg.org/#concept-headers-fill
          requiredArguments(
            'Headers.constructor tuple array argument',
            tuple.length,
            2
          );

          this.append(tuple[0], tuple[1]);
        }
      } else if (init) {
        for (const [ rawName, rawValue ] of Object.entries(init)) {
          this.append(rawName, rawValue);
        }
      }
    }

    this[headersGuard] = 'none';
  }

  // ref: https://fetch.spec.whatwg.org/#concept-headers-append
  append(name, value) {
    requiredArguments('Headers.append', arguments.length, 2);
    const [ newname, newvalue ] = normalizeParams(name, value);
    validateName(newname);
    validateValue(newvalue);

    if (this[headersGuard] === 'immutable') {
      throw new TypeError('Headers object is immutable');
    } else if (this[headersGuard] === 'request') {
      if (isForbiddenRequestHeaderName(name)) {
        return;
      }
    } else if (this[headersGuard] === 'request-no-cors') {
      let temporaryValue = dataGet(this[headersData], name);
      if (temporaryValue == null) {
        temporaryValue = value;
      } else {
        temporaryValue = `${temporaryValue}\x2c\x20${value}`;
      }
      if (!isNoCorsSafeListedRequestHeader(name, temporaryValue)) {
        return;
      }
    } else if (this[headersGuard] === 'response') {
      if (isForbiddenResponseHeaderName(name)) {
        return;
      }
    }

    dataAppend(this[headersData], newname, newvalue);

    if (this[headersGuard] === 'request-no-cors') {
      dataRemovePrivilegedNoCorsRequestHeaders(this[headersData]);
    }
  }

  delete(name) {
    requiredArguments('Headers.delete', arguments.length, 1);
    const [ newname ] = normalizeParams(name);
    validateName(newname);

    if (this[headersGuard] === 'immutable') {
      throw new TypeError('Headers object is immutable');
    } else if (this[headersGuard] === 'request') {
      if (isForbiddenRequestHeaderName(name)) {
        return;
      }
    } else if (this[headersGuard] === 'request-no-cors') {
      if (!isNoCorsSafeListedRequestHeader(name)) {
        return;
      }
      if (!privilegedNoCorsRequestHeaderName.includes(name)) {
        return;
      }
    } else if (this[headersGuard] === 'response') {
      if (isForbiddenResponseHeaderName(name)) {
        return;
      }
    }

    dataDelete(this[headersData], newname);

    if (this[headersGuard] === 'request-no-cors') {
      dataRemovePrivilegedNoCorsRequestHeaders(this[headersData]);
    }
  }

  get(name) {
    requiredArguments('Headers.get', arguments.length, 1);
    const [ newname ] = normalizeParams(name);
    validateName(newname);
    return dataGet(this[headersData], newname) ?? null;
  }

  has(name) {
    requiredArguments('Headers.has', arguments.length, 1);
    const [ newname ] = normalizeParams(name);
    validateName(newname);
    return dataHas(this[headersData], newname);
  }

  set(name, value) {
    requiredArguments('Headers.set', arguments.length, 2);
    const [ newName, newValue ] = normalizeParams(name, value);
    validateName(newName);
    validateValue(newValue);

    if (this[headersGuard] === 'immutable') {
      throw new TypeError('Headers object is immutable');
    } else if (this[headersGuard] === 'request') {
      if (isForbiddenRequestHeaderName(name)) {
        return;
      }
    } else if (this[headersGuard] === 'request-no-cors') {
      if (!isNoCorsSafeListedRequestHeader(name, value)) {
        return;
      }
    } else if (this[headersGuard] === 'response') {
      if (isForbiddenResponseHeaderName(name)) {
        return;
      }
    }

    dataSet(this[headersData], newName, newValue);

    if (this[headersGuard] === 'request-no-cors') {
      dataRemovePrivilegedNoCorsRequestHeaders(this[headersData]);
    }
  }

  get [Symbol.toStringTag]() {
    return 'Headers';
  }
}

class Headers extends DomIterableMixin(HeadersBase, headersData) {}

function isHeaders(value) {
  return value instanceof Headers;
}

wrapper.mod = {
  Headers,

  headersDataKey: headersData,
  headersGuardKey: headersGuard,
};
