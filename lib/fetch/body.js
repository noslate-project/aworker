'use strict';

const options = loadBinding('aworker_options');
const { ReadableStream, isReadableStreamDisturbed } = load('streams');
const { isTypedArray } = load('types');
const { TextEncoder, TextDecoder } = load('encoding');
const { Headers, headersGuardKey } = load('fetch/headers');
const { URL, URLSearchParams } = load('url');
const { createFollowingSignal } = load('dom/abort_signal');
const { bufferFromStream, hasHeaderValueOf } = load('fetch/body_helper');
const { Blob, bytesSymbol } = load('dom/file');
const { FormData, MultipartParser, MultipartBuilder } = load('fetch/form');

function getDefaultResponseType() {
  return options.has_location ? 'basic' : 'default';
}

function getDefaultRequestHeaderGuard() {
  return options.has_location ? 'request' : 'none';
}

function getDefaultResponseHeaderGuard() {
  return options.has_location ? 'response' : 'none';
}

function validateBodyType(owner, bodySource) {
  if (isTypedArray(bodySource)) {
    return true;
  } else if (bodySource instanceof ArrayBuffer) {
    return true;
  } else if (typeof bodySource === 'string') {
    return true;
  } else if (bodySource instanceof ReadableStream) {
    return true;
  } else if (bodySource instanceof FormData) {
    return true;
  } else if (bodySource instanceof URLSearchParams) {
    return true;
  } else if (!bodySource) {
    return true; // null body is fine
  }
  throw new Error(
    `Bad ${owner.constructor.name} body type: ${bodySource.constructor.name}`
  );
}

function bodyToArrayBuffer(bodySource) {
  const encoder = new TextEncoder();
  if (isTypedArray(bodySource)) {
    return bodySource.buffer;
  } else if (bodySource instanceof ArrayBuffer) {
    return bodySource;
  } else if (typeof bodySource === 'string') {
    return encoder.encode(bodySource).buffer;
  } else if (bodySource instanceof ReadableStream) {
    throw new Error(
      'Can\'t convert stream to ArrayBuffer (try bufferFromStream)'
    );
  } else if (
    bodySource instanceof FormData ||
    bodySource instanceof URLSearchParams
  ) {
    return encoder.encode(bodySource.toString()).buffer;
  } else if (!bodySource) {
    return new ArrayBuffer(0);
  }
  throw new Error(
    `Body type not implemented: ${bodySource.constructor.name}`
  );
}

const NULL_BODY_STATUS = [ 101, 204, 205, 304 ];
const REDIRECT_STATUS = [ 301, 302, 303, 307, 308 ];
const BodyUsedError =
  "Failed to execute 'clone' on 'Body': body is already used";

function byteUpperCase(s) {
  return String(s).replace(/[a-z]/g, function byteUpperCaseReplace(c) {
    return c.toUpperCase();
  });
}

function normalizeMethod(m) {
  const u = byteUpperCase(m);
  if (
    u === 'DELETE' ||
    u === 'GET' ||
    u === 'HEAD' ||
    u === 'OPTIONS' ||
    u === 'POST' ||
    u === 'PUT'
  ) {
    return u;
  }
  return m;
}

function getHeaderValueParams(value) {
  const params = new Map();
  // Forced to do so for some Map constructor param mismatch
  value
    .split(';')
    .slice(1)
    .map(s => s.trim().split('='))
    .filter(arr => arr.length > 1)
    .map(([ k, v ]) => [ k, v.replace(/^"([^"]*)"$/, '$1') ])
    .forEach(([ k, v ]) => params.set(k, v));
  return params;
}

const utf8Decoder = new TextDecoder('utf-8');
class Body {
  #contentType;
  #size;

  constructor(_bodySource, meta) {
    validateBodyType(this, _bodySource);
    this._bodySource = _bodySource;
    this.#contentType = meta.contentType;
    this.#size = meta.size;
    this._stream = null;
  }

  get body() {
    if (this._stream) {
      return this._stream;
    }

    if (!this._bodySource) {
      return null;
    } else if (this._bodySource instanceof ReadableStream) {
      this._stream = this._bodySource;
    } else {
      const buf = bodyToArrayBuffer(this._bodySource);
      if (!(buf instanceof ArrayBuffer)) {
        throw new Error(
          'Expected ArrayBuffer from body'
        );
      }

      this._stream = new ReadableStream({
        start(controller) {
          controller.enqueue(new Uint8Array(buf));
          controller.close();
        },
      });
    }

    return this._stream;
  }

  get bodyUsed() {
    if (this.body && isReadableStreamDisturbed(this.body)) {
      return true;
    }
    return false;
  }

  async text() {
    if (typeof this._bodySource === 'string') {
      return this._bodySource;
    }

    const ab = await this.arrayBuffer();
    return utf8Decoder.decode(ab);
  }

  async json() {
    const raw = await this.text();
    return JSON.parse(raw);
  }

  async blob() {
    return new Blob([ await this.arrayBuffer() ], {
      type: this.#contentType,
    });
  }

  // ref: https://fetch.spec.whatwg.org/#body-mixin
  async formData() {
    const formData = new FormData();
    if (hasHeaderValueOf(this.#contentType, 'multipart/form-data')) {
      const params = getHeaderValueParams(this.#contentType);

      // ref: https://tools.ietf.org/html/rfc2046#section-5.1
      const boundary = params.get('boundary');
      const body = new Uint8Array(await this.arrayBuffer());
      const multipartParser = new MultipartParser(body, boundary);

      return multipartParser.parse();
    } else if (
      hasHeaderValueOf(this.#contentType, 'application/x-www-form-urlencoded')
    ) {
      // From https://github.com/github/fetch/blob/master/fetch.js
      // Copyright (c) 2014-2016 GitHub, Inc. MIT License
      const body = await this.text();
      try {
        const params = new URLSearchParams(body);
        params.forEach((value, key) => {
          formData.append(key, value);
        });
      } catch (e) {
        throw new TypeError('Invalid form urlencoded format');
      }
      return formData;
    }
    throw new TypeError('Invalid form data');
  }

  arrayBuffer() {
    if (this._bodySource instanceof ReadableStream) {
      return bufferFromStream(this._bodySource.getReader(), this.#size);
    }
    return Promise.resolve(bodyToArrayBuffer(this._bodySource));
  }
}

class Request extends Body {
  constructor(input, init) {
    if (arguments.length < 1) {
      throw TypeError('Not enough arguments');
    }

    if (!init) {
      init = {};
    }

    let initBody;
    // prefer body from init
    if (init.body) {
      initBody = init.body;
    } else if (input instanceof Request && input._bodySource) {
      if (input.bodyUsed) {
        throw TypeError(BodyUsedError);
      }
      initBody = input._bodySource;
    } else if (typeof input === 'object' && 'body' in input && input.body) {
      if (input.bodyUsed) {
        throw TypeError(BodyUsedError);
      }
      initBody = input.body;
    } else {
      initBody = null;
    }

    let headers;
    // prefer headers from init
    if (init.headers) {
      headers = new Headers(init.headers);
    } else if (input instanceof Request) {
      headers = input.headers;
    } else {
      headers = new Headers();
    }
    headers[headersGuardKey] = getDefaultRequestHeaderGuard();

    let contentType;
    // See https://fetch.spec.whatwg.org/#bodyinit-unions
    // To extract a body and a `Content-Type` value from the initBody
    const encoder = new TextEncoder();
    if (initBody && !headers.has('content-type')) {
      if (typeof initBody === 'string') {
        initBody = encoder.encode(initBody);
        contentType = 'text/plain;charset=UTF-8';
      } else if (initBody instanceof ArrayBuffer) {
        initBody = new Uint8Array(initBody);
      } else if (initBody instanceof URLSearchParams) {
        initBody = encoder.encode(initBody.toString());
        contentType = 'application/x-www-form-urlencoded;charset=UTF-8';
      } else if (initBody instanceof Blob) {
        initBody = initBody[bytesSymbol];
        contentType = initBody.type;
      } else if (initBody instanceof FormData) {
        let boundary;
        if (headers.has('content-type')) {
          const params = getHeaderValueParams('content-type');
          boundary = params.get('boundary');
        }
        const multipartBuilder = new MultipartBuilder(
          init.body,
          boundary
        );
        initBody = multipartBuilder.getBody();
        contentType = multipartBuilder.getContentType();
      }
    }

    if (contentType != null && !headers.has('content-type')) {
      headers.set('content-type', contentType);
    } else {
      contentType = headers.get('content-type');
    }

    super(initBody, { contentType });
    this.headers = headers;

    // readonly attribute ByteString method;
    this.method = 'GET';

    // readonly attribute USVString url;
    this.url = '';

    // readonly attribute RequestCredentials credentials;
    this.credentials = 'omit';

    if (input instanceof Request) {
      if (input.bodyUsed) {
        throw TypeError(BodyUsedError);
      }
      this.method = input.method;
      this.url = input.url;
      this.headers = new Headers(input.headers);
      this.credentials = input.credentials;
      this._stream = input._stream;
    } else {
      this.url = new URL(String(input), globalThis.location.href).href;
    }

    if (init && 'method' in init && init.method) {
      this.method = normalizeMethod(init.method);
    }
    if (initBody != null && [ 'GET', 'HEAD' ].includes(this.method)) {
      throw new TypeError('Request with GET/HEAD method cannot have body.');
    }

    let signal;
    if (input instanceof Request) {
      signal = input.signal;
    } else if (init.signal) {
      signal = init.signal;
    }
    this.signal = createFollowingSignal(signal);

    if (
      init &&
      'credentials' in init &&
      init.credentials &&
      [ 'omit', 'same-origin', 'include' ].indexOf(init.credentials) !== -1
    ) {
      this.credentials = init.credentials;
    }
  }

  clone() {
    if (this.bodyUsed) {
      throw TypeError(BodyUsedError);
    }

    const iterators = this.headers.entries();
    const headersList = [];
    for (const header of iterators) {
      headersList.push(header);
    }

    let body2 = this._bodySource;

    if (this._bodySource instanceof ReadableStream) {
      const tees = this._bodySource.tee();
      this._stream = this._bodySource = tees[0];
      body2 = tees[1];
    }

    return new Request(this.url, {
      body: body2,
      method: this.method,
      headers: new Headers(headersList),
      credentials: this.credentials,
      signal: this.signal,
    });
  }
}

const responseDataWeakMap = new WeakMap();
class Response extends Body {
  constructor(body = null, init) {
    init = init ?? {};

    if (typeof init !== 'object') {
      throw new TypeError('\'init\' is not an object');
    }

    const extraInit = responseDataWeakMap.get(init) || {};
    let { type = getDefaultResponseType(), url = '' } = extraInit;

    let status = init.status === undefined ? 200 : Number(init.status || 0);
    let statusText = init.statusText ?? '';
    let headers = init.headers instanceof Headers
      ? init.headers
      : new Headers(init.headers);

    if (init.status !== undefined && (status < 200 || status > 599)) {
      throw new RangeError(
        `The status provided (${init.status}) is outside the range [200, 599]`
      );
    }

    // null body status
    if (body && NULL_BODY_STATUS.includes(status)) {
      throw new TypeError('Response with null body status cannot have body');
    }

    if (!type) {
      type = getDefaultResponseType();
    } else {
      if (type === 'error') {
        // spec: https://fetch.spec.whatwg.org/#concept-network-error
        status = 0;
        statusText = '';
        headers = new Headers();
        body = null;
        /* spec for other Response types:
         https://fetch.spec.whatwg.org/#concept-filtered-response-basic
         Please note that type "basic" is not the same thing as "default".*/
      } else if (type === 'basic') {
        for (const h of headers) {
          /* Forbidden Response-Header Names:
           https://fetch.spec.whatwg.org/#forbidden-response-header-name */
          if ([ 'set-cookie', 'set-cookie2' ].includes(h[0].toLowerCase())) {
            headers.delete(h[0]);
          }
        }
      } else if (type === 'cors') {
        /* CORS-safelisted Response-Header Names:
           https://fetch.spec.whatwg.org/#cors-safelisted-response-header-name */
        const allowedHeaders = [
          'Cache-Control',
          'Content-Language',
          'Content-Length',
          'Content-Type',
          'Expires',
          'Last-Modified',
          'Pragma',
        ].map(c => c.toLowerCase());
        for (const h of headers) {
          /* Technically this is still not standards compliant because we are
           supposed to allow headers allowed in the
           'Access-Control-Expose-Headers' header in the 'internal response'
           However, this implementation of response doesn't seem to have an
           easy way to access the internal response, so we ignore that
           header.
           TODO(serverhiccups): change how internal responses are handled
           so we can do this properly. */
          if (!allowedHeaders.includes(h[0].toLowerCase())) {
            headers.delete(h[0]);
          }
        }
        /* TODO(serverhiccups): Once I fix the 'internal response' thing,
         these actually need to treat the internal response differently */
      } else if (type === 'opaque' || type === 'opaqueredirect') {
        url = '';
        status = 0;
        statusText = '';
        headers = new Headers();
        body = null;
      }
    }
    headers[headersGuardKey] = getDefaultResponseHeaderGuard();

    const contentType = headers.get('content-type') || '';
    const size = Number(headers.get('content-length')) || undefined;

    super(body, { contentType, size });

    this.url = url;
    this.statusText = statusText;
    this.status = extraInit.status || status;
    this.headers = headers;
    this.redirected = extraInit.redirected || false;
    this.type = type;
  }

  get ok() {
    return this.status >= 200 && this.status < 300;
  }

  clone() {
    if (this.bodyUsed) {
      throw TypeError(BodyUsedError);
    }

    const iterators = this.headers.entries();
    const headersList = [];
    for (const header of iterators) {
      headersList.push(header);
    }

    let resBody = this._bodySource;

    if (this._bodySource instanceof ReadableStream) {
      const tees = this._bodySource.tee();
      this._stream = this._bodySource = tees[0];
      resBody = tees[1];
    }

    const responseInit = {
      status: this.status,
      statusText: this.statusText,
      headers: new Headers(headersList),
    };
    responseDataWeakMap.set(responseInit, {
      url: this.url,
    });
    return new Response(resBody, responseInit);
  }

  static redirect(url, status) {
    if (![ 301, 302, 303, 307, 308 ].includes(status)) {
      throw new RangeError(
        'The redirection status must be one of 301, 302, 303, 307 and 308.'
      );
    }
    return new Response(null, {
      status,
      statusText: '',
      headers: [[ 'Location', typeof url === 'string' ? url : url.toString() ]],
    });
  }
}

wrapper.mod = {
  Body,
  Request,
  Response,
  NULL_BODY_STATUS,
  REDIRECT_STATUS,
  responseDataWeakMap,
};
