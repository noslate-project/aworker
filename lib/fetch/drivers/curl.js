'use strict';

const curl = loadBinding('curl');
const {
  Options,
  Codes,
  CurlMulti,
  CurlEasy,
  easyStrErr,
  CURLPAUSE_CONT,
  CURL_READFUNC_PAUSE,
} = curl;

const { addCleanupHook } = load('process/execution');
const { createDeferred, bufferLikeToUint8Array } = load('utils');
const { userAgentHeaders, kCRLF } = load('fetch/constants');
const { debuglog, getDebuglogEnabled } = load('console/debuglog');
const { ReadableStream } = load('streams');
const { createAbortError } = load('dom/exception');
const {
  setTimeout,
} = load('timer');

const debug = debuglog('curl');

const kCodeNameMap = Object.create(null);
Object.getOwnPropertyNames(Codes).forEach(name => {
  kCodeNameMap[Codes[name]] = name;
});

let multi;
let initialized = false;
function getMulti() {
  if (!initialized) {
    debug('initialize multi handle');
    initialized = true;
    curl.init();
    multi = new CurlMulti();
    addCleanupHook(() => {
      multi.close();
      multi = null;
    });
  }
  if (multi == null) {
    throw new TypeError('Cannot fetch at process exiting.');
  }
  return multi;
}

function createCurlError(code, message) {
  const codeName = kCodeNameMap[code];
  const err = new TypeError(`Request failed (${codeName}): ${message}`);
  err.code = code;
  err.reason = message;
  return err;
}

// https://datatracker.ietf.org/doc/html/rfc2616#section-6.1
// Status-Line    = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
// HTTP-Version   = "HTTP" "/" 1*DIGIT "." 1*DIGIT
// Status-Code    = 3DIGIT
// Reason-Phrase  = *<TEXT, excluding CR, LF>
// SP             = <US-ASCII SP, space (32)>
// CR             = <US-ASCII CR, carriage return (13)>
// LF             = <US-ASCII LF, linefeed (10)>
// TEXT           = <any OCTET except CTLs, but including LWS>
// CTL            = <any US-ASCII control character (octets 0 - 31) and DEL (127)>
// LWS            = [CRLF] 1*( SP | HT )
// HT             = <US-ASCII HT, horizontal-tab (9)>
//
// https://datatracker.ietf.org/doc/html/rfc2616#section-4.2
// message-header = field-name ":" [ field-value ]
// field-name     = token
// field-value    = *( field-content | LWS )
// field-content  = <the OCTETs making up the field-value
//                  and consisting of either *TEXT or combinations
//                  of token, separators, and quoted-string>
function parseHeaderLines(lines) {
  const [ /* httpVersion */, statusCode, reason ] = lines[0].split(' ');
  const status = Number.parseInt(statusCode, 10);
  const statusText = reason.substring(0, -2);
  const headers = [];
  for (let idx = 1; idx < lines.length; idx++) {
    const colonIdx = lines[idx].indexOf(':');
    if (colonIdx === -1) {
      continue;
    }
    headers.push([ lines[idx].substring(0, colonIdx).trim(), lines[idx].substring(colonIdx + 1).trim() ]);
  }
  return {
    status,
    statusText,
    headers,
  };
}

class Curl {
  #handle = new CurlEasy();

  #remoteIp;
  #remotePort;
  #remoteFamily;

  #headerDeferred = createDeferred();
  #headerReceived = false;
  #responseHeaderLines = [];

  #responseBodyController;
  #responseBody;

  #body;
  #bodyReader;
  #bodyBuffers = [];
  #bodyDone = false;
  #bodyPaused = false;

  #timingInfo = {
    startTime: Date.now(),
  };

  #onPreReq = (remoteIp, localIp, remotePort, localPort) => {
    debug('pre request remote(%s, %d) from local(%s, %d)', remoteIp, remotePort, localIp, localPort);
    if (remoteIp == null) {
      return;
    }
    this.#remoteIp = remoteIp;
    this.#remotePort = remotePort;
    // Naive category.
    this.#remoteFamily = remoteIp.indexOf(':') >= 0 ? 'IPv6' : 'IPv4';
  };

  #onHeader = line => {
    debug('handle header', line);
    if (line === kCRLF) {
      const { status, statusText, headers } = parseHeaderLines(this.#responseHeaderLines);
      if (status === 100) {
        this.#responseHeaderLines = [];
        return;
      }
      if (this.#remoteIp != null) {
        headers.push([ 'x-anc-remote-address', `${this.#remoteIp}` ],
          [ 'x-anc-remote-port', `${this.#remotePort}` ],
          [ 'x-anc-remote-family', `${this.#remoteFamily}` ]
        );
      }
      this.#headerReceived = true;
      this.#headerDeferred.resolve({
        status, statusText, headers,
      });
      return;
    }
    this.#responseHeaderLines.push(line);
  };

  #onRead = (nread, ab) => {
    debug('handle onRead', nread);
    const read = this.read(nread, new Uint8Array(ab));
    debug('handle onRead done', read);
    return read;
  };

  #onWrite = ab => {
    debug('handle onWrite', ab.byteLength);
    this.#responseBodyController.enqueue(ab);
  };

  #onDone = (code, ...timingInfo) => {
    debug('handle onDone', code);
    const multi = getMulti();
    multi.removeHandle(this.#handle);
    this.#handle = null;

    const [ totalTime, namelookupTime, connectTime, appconnectTime, pretransferTime, starttransferTime, redirectTime ] = timingInfo;

    this.#timingInfo.totalTime = totalTime / 1000;
    this.#timingInfo.namelookupTime = namelookupTime / 1000;
    this.#timingInfo.connectTime = connectTime / 1000;
    this.#timingInfo.appconnectTime = appconnectTime / 1000;
    this.#timingInfo.pretransferTime = pretransferTime / 1000;
    this.#timingInfo.starttransferTime = starttransferTime / 1000;
    this.#timingInfo.redirectTime = redirectTime / 1000;

    if (this.#bodyReader && !this.#bodyDone) {
      this.#bodyReader.cancel();
    }

    if (code === Codes.CURLE_OK) {
      this.#responseBodyController.close();
      return;
    }
    const message = easyStrErr(code);
    const err = createCurlError(code, message);
    if (this.#headerReceived) {
      this.#responseBodyController.error(err);
    } else {
      const err = createCurlError(code, message);
      this.#headerDeferred.reject(err);
    }
  };

  #abort = err => {
    if (this.#handle == null) {
      // Request is either aborted or done.
      return;
    }
    this.#bodyReader?.cancel(err)
      .catch(err => {
        debug('cancel body reader failed', err);
      });
    this.#responseBodyController.error(err);
    this.#headerDeferred.reject(err);
    multi.removeHandle(this.#handle);
    this.#handle = null;
  };

  constructor(url, method, headers, body, signal) {
    const multi = getMulti();
    this.#handle = new CurlEasy();
    this.#handle._onPreReq = this.#onPreReq;
    this.#handle._onHeader = this.#onHeader;
    this.#handle._onRead = this.#onRead;
    this.#handle._onWrite = this.#onWrite;
    this.#handle._onDone = this.#onDone;

    if (getDebuglogEnabled('curl')) {
      this.#handle.setOpt(Options.CURLOPT_VERBOSE, 1);
    }

    // Ignore proxy connect headers.
    this.#handle.setOpt(Options.CURLOPT_SUPPRESS_CONNECT_HEADERS, 1);
    this.#handle.setOpt(Options.CURLOPT_URL, url);
    const hasBody = body != null;
    this.setMethod(method, hasBody);
    this.setHeaders(headers);
    if (hasBody) {
      // TODO(chengzhong.wcz): should infer content-length from body type.
      // Tell curl to disable chunked transfer-encoding.
      let contentLength = headers.get('content-length');
      if (contentLength != null) {
        contentLength = Number.parseInt(contentLength);
        this.#handle.setOpt(Options.CURLOPT_POSTFIELDSIZE, contentLength);
      }
      this.#body = body;
      this.drainBody();
    }

    this.#responseBody = new ReadableStream({
      start: controller => {
        this.#responseBodyController = controller;
      },
    });
    this.#headerDeferred = createDeferred();

    multi.addHandle(this.#handle);

    if (signal != null) {
      // https://fetch.spec.whatwg.org/#abort-fetch
      signal.addEventListener('abort', () => {
        const err = createAbortError();
        this.#abort(err);
      });
    }
  }

  setMethod(method, hasBody) {
    if (method === 'HEAD') {
      this.#handle.setOpt(Options.CURLOPT_NOBODY, 1);
    } else if (hasBody) {
      this.#handle.setOpt(Options.CURLOPT_POST, 1);
    } else {
      this.#handle.setOpt(Options.CURLOPT_HTTPGET, 1);
    }
    this.#handle.setOpt(Options.CURLOPT_CUSTOMREQUEST, method);
  }

  setHeaders(headers) {
    const headerArray = [];
    for (const pair of headers) {
      headerArray.push(`${pair[0]}: ${pair[1]}`);
    }
    for (const [ key, defaultVal ] of userAgentHeaders) {
      if (!headers.has(key)) {
        headerArray.push(`${key}: ${defaultVal}`);
      }
    }
    this.#handle.setOpt(Options.CURLOPT_HTTPHEADER, headerArray);
  }

  drainBody() {
    this.#bodyReader = this.#body.getReader();
    const drain = ({ done, value }) => {
      // curl_easy_pause can invoke write callbacks synchronously, defer to
      // next loop tick.
      let unpause = Promise.resolve();
      if (this.#bodyPaused) {
        unpause = new Promise(resolve => {
          setTimeout(() => {
            this.#bodyPaused = false;
            // If the #handle is null, request is either aborted or done.
            this.#handle?.pause(CURLPAUSE_CONT);
            resolve();
          }, 1);
        });
      }
      if (done) {
        this.#bodyDone = true;
        return unpause;
      }
      const u8 = bufferLikeToUint8Array(value);
      this.#bodyBuffers.push(u8);
      return unpause.then(() => this.#bodyReader.read()).then(drain);
    };
    this.#bodyReader.read()
      .then(drain)
      .catch(async err => {
        const error = new TypeError('Failed to drain request body');
        error.cause = err;
        this.#abort(error);
      });
  }

  /**
   *
   * @param {number} byteLength
   * @param {Uint8Array} target
   * @return
   */
  read(byteLength, target) {
    if (byteLength === 0) {
      return 0;
    }
    if (this.#bodyBuffers.length === 0) {
      if (this.#bodyDone) {
        return 0;
      }
      this.#bodyPaused = true;
      return CURL_READFUNC_PAUSE;
    }
    let readByteLength = 0;
    while (readByteLength < byteLength && this.#bodyBuffers.length) {
      const next = this.#bodyBuffers.shift();
      if (readByteLength + next.byteLength > byteLength) {
        const toBeReadByteLength = byteLength - readByteLength;
        const toBeReadView = new Uint8Array(next.buffer, next.byteOffset, toBeReadByteLength);
        const remaining = new Uint8Array(next.buffer, next.byteOffset + toBeReadByteLength, next.byteLength - toBeReadByteLength);
        target.set(toBeReadView, readByteLength);
        this.#bodyBuffers.unshift(remaining);
        readByteLength += toBeReadByteLength;
        break;
      }
      target.set(next, readByteLength);
      readByteLength += next.byteLength;
    }

    if (readByteLength !== 0) {
      return readByteLength;
    }
    if (this.#bodyDone) {
      return 0;
    }
    this.#bodyPaused = true;
    return CURL_READFUNC_PAUSE;
  }

  async getResponseHeader() {
    const ret = await this.#headerDeferred.promise;
    // MakeCallback drains microtask queue. CurlMulti cannot be invoked recursively.
    await new Promise(resolve => setTimeout(resolve, 0));

    return ret;
  }

  getResponseBody() {
    return this.#responseBody;
  }

  get timingInfo() {
    return this.#timingInfo;
  }
}

async function sendFetchReq(url, method, headers, body, signal) {
  debug('request(url: %s, method: %s) with options', url, method, headers);
  const request = new Curl(url, method, headers, body, signal);
  const { status, statusText, headers: responseHeaders } = await request.getResponseHeader();
  debug('request(url: %s, method: %s) received response header', url, method, status, responseHeaders);

  return {
    status,
    statusText,
    headers: responseHeaders,
    body: request.getResponseBody(),
    timingInfo: request.timingInfo,
  };
}

wrapper.mod = {
  sendFetchReq,
};
