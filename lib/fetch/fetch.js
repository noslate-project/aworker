'use strict';

const { createAbortError } = load('dom/exception');
const {
  Request,
  Response,
  REDIRECT_STATUS,
  responseDataWeakMap,
} = load('fetch/body');
const { URL } = load('url');
const options = loadBinding('aworker_options');
const { allowedProtocols } = load('fetch/constants');
const { performance } = load('performance/performance');
const { markResourceTiming } = load('performance/resource_timing');

function getDriver() {
  if (!options.no_experimental_curl_fetch) {
    return load('fetch/drivers/curl').sendFetchReq;
  }
  return load('fetch/drivers/agent').sendFetchReq;
}

// https://fetch.spec.whatwg.org/#fetch-method
async function fetch(input, init) {
  const sendFetchReq = getDriver();
  const request = new Request(input, init);
  const method = request.method;
  const headers = request.headers;
  const body = request.body;
  const signal = request.signal;
  let url = request.url;

  if (signal.aborted) {
    throw createAbortError();
  }

  let redirected = false;
  let remRedirectCount = 20; // TODO: use a better way to handle

  let responseBody;
  let responseInit = {};
  while (remRedirectCount) {
    if (!allowedProtocols.includes(new URL(url).protocol)) {
      throw new TypeError('Unsupported protocols');
    }
    const fetchResponse = await sendFetchReq(
      url,
      method,
      headers,
      body,
      signal
    );

    responseInit = {
      status: 200,
      statusText: fetchResponse.statusText,
      headers: fetchResponse.headers,
    };
    responseBody = fetchResponse.body;

    responseDataWeakMap.set(responseInit, {
      redirected,
      status: fetchResponse.status,
      url,
    });

    const response = new Response(responseBody, responseInit);

    if (REDIRECT_STATUS.includes(fetchResponse.status)) {
      // We're in a redirect status
      switch ((init && init.redirect) || 'follow') {
        case 'error':
          responseInit = {};
          responseDataWeakMap.set(responseInit, {
            type: 'error',
            redirected: false,
            url: '',
          });
          return new Response(null, responseInit);
        case 'manual':
          // On the web this would return a `opaqueredirect` response, but
          // those don't make sense server side. See
          // https://github.com/denoland/deno/issues/8351.
          return response;
        case 'follow':
        // fallthrough
        default: {
          let redirectUrl = response.headers.get('Location');
          if (redirectUrl == null) {
            return response; // Unspecified
          }
          if (
            !redirectUrl.startsWith('http://') &&
            !redirectUrl.startsWith('https://')
          ) {
            redirectUrl = new URL(redirectUrl, url).href;
          }
          url = redirectUrl;
          redirected = true;
          remRedirectCount--;
        }
      }
    } else {
      const timingInfo = createOpaqueTimingInfo({ startTime: performance.now() });
      markResourceTiming(timingInfo, url, 'fetch', globalThis, '');
      return response;
    }
  }

  responseDataWeakMap.set(responseInit, {
    type: 'error',
    redirected: false,
    url: '',
  });

  return new Response(null, responseInit);
}

function createOpaqueTimingInfo(timingInfo) {
  return {
    startTime: timingInfo.startTime ?? 0,
    redirectStartTime: 0,
    redirectEndTime: 0,
    postRedirectStartTime: timingInfo.startTime ?? 0,
    finalServiceWorkerStartTime: 0,
    finalNetworkResponseStartTime: 0,
    finalNetworkRequestStartTime: 0,
    endTime: 0,
    encodedBodySize: 0,
    decodedBodySize: 0,
    finalConnectionTimingInfo: null,
  };
}

wrapper.mod = {
  fetch,
};
