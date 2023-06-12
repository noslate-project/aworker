'use strict';

const { Request, Response } = load('fetch/body');
const { headersGuardKey } = load('fetch/headers');
const { URL } = load('url');
const { fetch } = load('fetch/fetch');
const { AbortController } = load('dom/abort_controller');
const { createInvalidStateError } = load('dom/exception');
const agentChannel = load('agent_channel');
const binding = loadBinding('cache');
const options = loadBinding('aworker_options');
const debug = load('console/debuglog').debuglog('cache');
const {
  private_symbols: {
    platform_private_options_symbol,
  },
} = loadBinding('constants');

const allowedProtocols = [ 'http:', 'https:' ];
const batchOperationTypes = [ 'delete', 'put' ];
const responseArrayBufferStoreKey = Symbol('cache response arraybuffer key');

let inited = false;
function cachePreamble() {
  if (!options.has_same_origin_shared_data_dir) {
    throw new Error('Worker is not bootstrapped with --same-origin-shared-data');
  }
  if (inited === false) {
    binding.init();
    inited = true;
  }
}

function resourceIdForCacheStorage() {
  return '[CacheStorage]';
}

function resourceIdForCache(cacheName) {
  return `[CachePage]${cacheName}`;
}

function keyValuePairsToTuples(arr) {
  return arr.map(pairs => [ pairs.key, pairs.value ]);
}

function tuplesToKeyValuePairs(arr) {
  return arr.map(it => ({ key: it[0], value: it[1] }));
}

async function sharedResourceFetch(cache, request, options, abortController) {
  const resourceId = `${request.method} ${request.url}`;
  const resource = await agentChannel.acquireResource(resourceId, /** exclusive */true);
  try {
    const it = await cache.matchAll(request);
    if (it.length !== 0) {
      return;
    }
    debug('shared resource(%s) fetch(%s, %s)', resourceId, request.method, request.url);
    const response = await fetch(request, options);
    try {
      validateResponse(response);
    } catch (e) {
      abortController.abort();
      throw e;
    }
    const body = await response.arrayBuffer();
    response[platform_private_options_symbol] = response;
    return new Response(body, response);
  } finally {
    resource.release();
  }
}

async function listCacheKeys(cacheName) {
  // Read op, non-exclusive
  const stub = await agentChannel.acquireResource(resourceIdForCache(cacheName), /** exclusive */false);
  try {
    const page = await new Promise(resolve => {
      binding.readCachePage(cacheName, (error, page) => {
        if (error) {
          debug('failed to read cache page', cacheName, error);
          return resolve([]);
        }
        resolve(page);
      });
    });
    const requests = [];
    for (const item of page) {
      requests.push(new Request(item.url, {
        method: item.req_method,
        headers: keyValuePairsToTuples(item.req_headers),
      }));
    }
    return requests;
  } finally {
    stub.release();
  }
}

/**
 *
 * @param {string} cacheName -
 * @param {bool} [inTransaction=false] outer managed transaction
 */
async function readCacheFullStorage(cacheName, inTransaction = false) {
  let stub;
  if (!inTransaction) {
    // Read op, non-exclusive
    stub = await agentChannel.acquireResource(resourceIdForCache(cacheName), /** exclusive */false);
  }
  try {
    const page = await new Promise(resolve => {
      binding.readCachePage(cacheName, (error, page) => {
        if (error) {
          debug('failed to read cache page', cacheName, error);
          return resolve([]);
        }
        resolve(page);
      });
    });

    const storage = [];
    for (const item of page) {
      const request = new Request(item.url, {
        method: item.req_method,
        headers: keyValuePairsToTuples(item.req_headers),
      });
      const responseData = await new Promise((resolve, reject) => {
        binding.readCacheObjectPage(cacheName, item.cache_object_filename, (error, data) => {
          if (error) {
            return reject(error);
          }
          resolve(data.response);
        });
      });
      // TODO: empty data
      if (responseData.url === '' && responseData.status === 0) {
        continue;
      }
      const responseInit = {
        status: responseData.status,
        headers: keyValuePairsToTuples(responseData.headers),
      };
      responseInit[platform_private_options_symbol] = {
        url: responseData.url,
      };
      const response = new Response(responseData.body, responseInit);
      response[responseArrayBufferStoreKey] = responseData.body;
      storage.push([ request, response ]);
    }
    return storage;
  } finally {
    await stub?.release();
  }
}

/**
 * Transactions shall be managed in outer scope
 * @param {string} cacheName -
 * @param {[Request, Response][]} storage -
 */
async function writeCachePage(cacheName, storage) {
  return new Promise(resolve => {
    const items = [];
    for (const [ request, response ] of storage) {
      items.push({
        url: request.url,
        req_method: request.method,
        req_headers: tuplesToKeyValuePairs(Array.from(request.headers.entries())),
        vary: response.headers.get('vary') ?? '',
      });
    }
    binding.writeCachePage(cacheName, items, () => {
      // TODO: error handling
      resolve();
    });
  });
}

/**
 * Transactions shall be managed in outer scope
 * @param {string} cacheName -
 * @param {[Request, Response][]} storage -
 */
function writeCacheObjectPages(cacheName, storage) {
  const futures = [];
  for (const [ request, response ] of storage) {
    const future = new Promise(resolve => {
      binding.writeCacheObjectPage(cacheName, {
        url: request.url,
        method: request.method,
        headers: tuplesToKeyValuePairs(Array.from(request.headers.entries())),
      }, {
        url: response.url,
        status: response.status,
        headers: tuplesToKeyValuePairs(Array.from(response.headers.entries())),
        body: new Uint8Array(response[responseArrayBufferStoreKey]),
      }, () => {
        // TODO: error handling
        resolve();
      });
    });
    futures.push(future);
  }
  return futures;
}

function isOkStatus(status) {
  // eslint-disable-next-line yoda
  if (200 <= status && status <= 299) {
    return true;
  }
  return false;
}

function getOption(options, name, defaults = false) {
  return options?.[name] ?? defaults;
}

function getHeadersFieldValues(headers, field) {
  return headers.get(field).split(',').map(it => it.trim());
}

/**
 *
 * @param {Request} requestQuery -
 * @param {Request} cachedRequest -
 * @param {Response} cachedResponse -
 * @param {*} options -
 */
function requestMatchCachedItem(requestQuery, cachedRequest, cachedResponse = null, options) {
  if (getOption(options, 'ignoreMethod') === false && requestQuery.method !== 'GET') {
    return false;
  }
  const queryUrl = new URL(requestQuery.url);
  const cachedUrl = new URL(cachedRequest.url);
  if (getOption(options, 'ignoreSearch')) {
    queryUrl.search = '';
    cachedUrl.search = '';
  }
  queryUrl.hash = '';
  cachedUrl.hash = '';
  if (queryUrl.toString() !== cachedUrl.toString()) {
    return false;
  }
  if (cachedResponse === null || getOption(options, 'ignoreVary') || !cachedResponse.headers.has('vary')) {
    return true;
  }
  const fieldValues = getHeadersFieldValues(cachedResponse.headers, 'vary');
  for (const fv of fieldValues) {
    if (fv === '*') {
      return false;
    }
    if (requestQuery.headers.get(fv) !== cachedRequest.headers.get(fv)) {
      return false;
    }
  }
  return true;
}

function queryCache(storage, requestQuery, options) {
  const resultList = [];
  for (const [ cachedRequest, cachedResponse ] of storage) {
    if (requestMatchCachedItem(requestQuery, cachedRequest, cachedResponse, options)) {
      resultList.push([ cachedRequest.clone(), cachedResponse.clone() ]);
    }
  }
  return resultList;
}

function batchCacheOperationsAtomicSteps(storage, operations) {
  const addedItems = [];
  const resultList = [];
  for (const op of operations) {
    if (!batchOperationTypes.includes(op.type)) {
      throw new TypeError('Invalid cache operation type');
    }
    if (op.type === 'delete' && op.response != null) {
      throw new TypeError('Invalid cache delete operation');
    }
    if (queryCache(addedItems, op.request, op.options).length !== 0) {
      throw createInvalidStateError();
    }

    if (op.type === 'delete') {
      for (let idx = storage.length - 1; idx !== -1; idx--) {
        const [ cachedRequest, cachedResponse ] = storage[idx];
        if (requestMatchCachedItem(op.request, cachedRequest, cachedResponse, op.options)) {
          storage.splice(idx, 1);
        }
      }
    } else if (op.type === 'put') {
      if (op.response == null) {
        throw new TypeError('Cache put operation with nil response');
      }
      const protocol = new URL(op.request.url).protocol;
      if (!allowedProtocols.includes(protocol)) {
        throw new TypeError(`Unsupported protocol '${protocol}' in cache`);
      }
      if (op.request.method !== 'GET') {
        throw new TypeError('Unable to cache requests other than GET method');
      }
      if (op.options != null) {
        throw new TypeError('Invalid cache put operation');
      }

      for (let idx = storage.length - 1; idx !== -1; idx--) {
        const [ cachedRequest, cachedResponse ] = storage[idx];
        if (requestMatchCachedItem(op.request, cachedRequest, cachedResponse, op.options)) {
          storage.splice(idx, 1);
        }
      }
      // TODO: check quota;
      debug('put request(%s %s)', op.request.method, op.request.url);
      storage.push([ op.request, op.response ]);
      addedItems.push([ op.request, op.response ]);
    }

    resultList.push([ op.request, op.response ]);
  }
}

async function batchCacheOperations(cacheName, operations) {
  for (const op of operations) {
    if (op.type === 'put' && op.response != null) {
      // Response should have been exhausted and no network operation is required.
      op.response[responseArrayBufferStoreKey] = await op.response.arrayBuffer();
    }
  }

  // Write op, exclusive.
  const stub = await agentChannel.acquireResource(resourceIdForCache(cacheName), /** exclusive */true);
  try {
    const storage = await readCacheFullStorage(cacheName, true);
    let resultList;
    try {
      resultList = batchCacheOperationsAtomicSteps(storage, operations);
    } catch (e) {
      throw e;
    }
    await writeCachePage(cacheName, storage);
    await Promise.allSettled(writeCacheObjectPages(cacheName, storage));
    return resultList;
  } finally {
    await stub.release();
  }
}

/**
 *
 * @param {Request} request -
 */
function validateRequest(request) {
  if (request == null) {
    return;
  }
  if (!allowedProtocols.includes(new URL(request.url).protocol)) {
    throw new TypeError('Expect a request with http/https url');
  }
}

/**
 *
 * @param {Response} response -
 */
function validateResponse(response) {
  if (response.type === 'error' || !isOkStatus(response.status) || response.status === 206) {
    const err = new TypeError(`Error response is not cache-able: status(${response.status})`);
    err.response = response;
    throw err;
  }
  if (response.headers.has('vary')) {
    const fieldValues = getHeadersFieldValues(response.headers, 'vary');
    for (const fv of fieldValues) {
      if (fv === '*') {
        const err = new TypeError('Vary with "*" is not cache-able.');
        err.response = response;
        throw err;
      }
    }
  }
}

class Cache {
  /** @type {string} */
  #name;
  constructor(name) {
    this.#name = name;
  }

  async match(request, options) {
    cachePreamble();
    const responses = await this.matchAll(request, options);
    if (responses.length === 0) {
      return undefined;
    }
    return responses[0];
  }

  async matchAll(request, options) {
    cachePreamble();
    /** @type {Request} */
    let r;
    if (request instanceof Request) {
      r = request;
    } else if (typeof request === 'string') {
      r = new Request(request);
    }
    validateRequest(r);

    const responses = [];
    const list = await readCacheFullStorage(this.#name);
    if (request == null) {
      for (const [ , resp ] of list) {
        responses.push(resp.clone());
      }
    } else {
      const requestResponses = queryCache(list, r, options);
      responses.push(...requestResponses.map(it => it[1]));
    }

    // TODO: Check cors policy?
    for (const resp of responses) {
      resp.headers[headersGuardKey] = 'immutable';
    }
    // TODO: FrozenArray impl
    return responses;
  }

  async add(request) {
    cachePreamble();
    return this.addAll([ request ]);
  }

  async addAll(requests) {
    cachePreamble();
    const responsePromises = [];
    const requestList = [];
    const abortController = new AbortController();

    for (const request of requests) {
      let r;
      if (request instanceof Request) {
        r = request;
      } else if (typeof request === 'string') {
        r = new Request(request);
      }
      validateRequest(r);
      requestList.push(r);
      const responsePromise = sharedResourceFetch(this, r, { signal: abortController.signal }, abortController);
      // TODO: https://fetch.spec.whatwg.org/#process-response-end-of-body
      responsePromises.push(responsePromise);
    }
    const responses = await Promise.all(responsePromises);
    const operations = [];
    for (const [ idx, resp ] of responses.entries()) {
      // When the request is handled by shared resource management.
      if (resp == null) {
        continue;
      }
      const op = {
        type: 'put',
        request: requestList[idx],
        response: resp,
      };
      operations.push(op);
    }
    if (operations.length === 0) {
      return;
    }
    await batchCacheOperations(this.#name, operations);
  }

  async put(request, response) {
    cachePreamble();
    let r;
    if (request instanceof Request) {
      r = request;
    } else if (typeof request === 'string') {
      r = new Request(request);
    }
    validateRequest(r);
    validateResponse(response);
    if (response.bodyUsed) {
      throw new TypeError();
    }
    const clonedResponse = response.clone();
    // lock the input response;
    await response.arrayBuffer();
    await batchCacheOperations(this.#name, [
      {
        type: 'put',
        request: r,
        response: clonedResponse,
      },
    ]);
  }

  async delete(request, options) {
    cachePreamble();
    let r;
    if (request instanceof Request) {
      r = request;
    } else if (typeof request === 'string') {
      r = new Request(request);
    }
    validateRequest(r);
    await batchCacheOperations(this.#name, [
      {
        type: 'delete',
        request: r,
        options,
      },
    ]);
  }

  async keys(request, options) {
    cachePreamble();
    let r;
    if (request instanceof Request) {
      r = request;
    } else if (typeof request === 'string') {
      r = new Request(request);
    }

    let requests;
    if (r == null) {
      requests = await listCacheKeys(this.#name);
    } else {
      requests = [];
      const list = await readCacheFullStorage(this.#name);
      const requestResponses = queryCache(list, r, options);
      for (const [ cachedRequest ] of requestResponses) {
        requests.push(cachedRequest);
      }
    }

    for (const item of requests) {
      item.headers[headersGuardKey] = 'immutable';
    }
    // TODO: FrozenArray impl
    return requests;
  }
}

class CacheStorage {
  async match(request, options) {
    cachePreamble();
    const cacheNames = await this.keys();
    for (const cacheName of cacheNames) {
      const cache = new Cache(cacheName);
      const matched = await cache.match(request, options);
      if (matched) {
        return matched;
      }
    }
  }

  async has(cacheName) {
    cachePreamble();
    cacheName = String(cacheName);
    const resource = await agentChannel.acquireResource(resourceIdForCacheStorage(), /** exclusive */false);
    try {
      return new Promise(resolve => {
        binding.detectCacheStorage(cacheName, exists => {
          resolve(exists);
        });
      });
    } finally {
      resource.release();
    }
  }

  async open(cacheName) {
    cachePreamble();
    cacheName = String(cacheName);
    const resource = await agentChannel.acquireResource(resourceIdForCacheStorage(), /** exclusive */true);
    try {
      return new Promise(resolve => {
        binding.ensureCacheStorage(cacheName, () => {
          const cache = new Cache(cacheName);
          resolve(cache);
        });
      });
    } finally {
      resource.release();
    }
  }

  async delete(cacheName) {
    cachePreamble();
    cacheName = String(cacheName);
    const resource = await agentChannel.acquireResource(resourceIdForCacheStorage(), /** exclusive */true);
    try {
      return new Promise(resolve => {
        binding.deleteCacheStorage(cacheName, () => {
          resolve();
        });
      });
    } finally {
      resource.release();
    }
  }

  async keys() {
    cachePreamble();
    const resource = await agentChannel.acquireResource(resourceIdForCacheStorage(), /** exclusive */false);
    try {
      return new Promise(resolve => {
        binding.listCacheStorage(cacheNames => {
          resolve(cacheNames);
        });
      });
    } finally {
      resource.release();
    }
  }
}

wrapper.mod = {
  Cache,
  CacheStorage,
};
