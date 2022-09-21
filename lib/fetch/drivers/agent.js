'use strict';

const process = loadBinding('process');
const agentChannel = load('agent_channel');
const { createDeferred } = load('utils');
const { userAgentHeaders } = load('fetch/constants');
const { createAbortError } = load('dom/exception');
const { NULL_BODY_STATUS, REDIRECT_STATUS } = load('fetch/body');

let _nextRequestId = 0;
function nextRequestId() {
  if (_nextRequestId === Number.MAX_SAFE_INTEGER) {
    _nextRequestId = 0;
  }
  return _nextRequestId++;
}
async function sendFetchReq(url, method, headers, body, signal) {
  if (!process._hasAgent) {
    throw new Error('`fetch()` only enabled under agent IPC mode');
  }

  const headerArray = [];
  for (const pair of headers) {
    headerArray.push(pair[0], pair[1]);
  }
  for (const [ key, defaultVal ] of userAgentHeaders) {
    if (!headers.has(key)) {
      headerArray.push(key, defaultVal);
    }
  }

  const hasBody = body != null;
  let bodyReader;
  const args = {
    method,
    url,
    headers: headerArray,
    requestId: nextRequestId(),
  };
  let sid;
  if (hasBody) {
    const result = await agentChannel.call('streamOpen', {});
    sid = args.sid = result.sid;
  }
  const headerResponseFuture = agentChannel.call('fetch', args);
  if (hasBody) {
    // Acquire the stream.
    bodyReader = body.getReader();
    agentChannel.pipeStreamsToAgent(sid, body, bodyReader);
  }
  let responseBody;

  const deferred = createDeferred();
  if (signal != null) {
    // https://fetch.spec.whatwg.org/#abort-fetch
    signal.addEventListener('abort', () => {
      const err = createAbortError();
      if (hasBody) {
        bodyReader.cancel(err);
      }
      if (responseBody) {
        agentChannel.abortReadableStream(responseBody, err);
      }
      deferred.reject(err);
      agentChannel.call('fetchAbort', { requestId: args.requestId }).catch(() => {});
    });
  }

  const headerResponse = await Promise.race([ headerResponseFuture, deferred.promise ]);
  const {
    // basically, this will be the same value of above manually opened writable
    // stream id.
    sid: responseSid,
    status,
    statusText,
    headers: responseRawHeaders,
  } = headerResponse;

  // TODO: aborted?

  const responseHeaders = [];
  for (let idx = 0; idx < responseRawHeaders.length; idx += 2) {
    responseHeaders.push([ responseRawHeaders[idx], responseRawHeaders[idx + 1] ]);
  }

  if (
    NULL_BODY_STATUS.includes(status) ||
    REDIRECT_STATUS.includes(status)
  ) {
    // We won't use body of received response.
    responseBody = null;
  } else {
    responseBody = agentChannel.createReadableStream(responseSid);
  }

  return {
    status,
    statusText,
    headers: responseHeaders,
    body: responseBody,
  };
}

wrapper.mod = {
  sendFetchReq,
};
