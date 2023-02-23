'use strict';

const { getEventInternalData: $, Event } = load('dom/event');
const { Request, Response } = load('fetch/body');
const { ReadableStream } = load('streams');
const { createInvalidStateError } = load('dom/exception');
const { createDeferred } = load('utils');
const { createBaggages } = load('service_worker/baggages');

class ExtendableEvent extends Event {
  constructor(type, eventInitDict) {
    super(type, eventInitDict);
    $(this).extendLifetimeDeferred = createDeferred();
    $(this).extendLifetimePromises = [];
    $(this).pendingPromisesCount = 0;
    $(this).timedOutFlag;
  }

  get #active() {
    const data = $(this);
    return data.timedOutFlag === undefined || data.pendingPromisesCount > 0 || data.dispatchFlag;
  }

  async waitUntil(future) {
    const data = $(this);
    if (!data.isTrusted) {
      throw createInvalidStateError();
    }
    if (!this.#active) {
      throw createInvalidStateError();
    }
    future = Promise.resolve(future);
    data.extendLifetimePromises.push(future);
    data.pendingPromisesCount++;
    future.then(
      () => {
        data.pendingPromisesCount--;
        if (data.pendingPromisesCount === 0) {
          data.extendLifetimeDeferred.resolve();
        }
      },
      err => {
        data.pendingPromisesCount--;
        data.extendLifetimeDeferred.reject(err);
      });
  }
}

function createExtendableEvent(type) {
  const event = new ExtendableEvent(type);
  return event;
}

function getExtendableEventExtendedLifetimePromise(extendableEvent) {
  const data = $(extendableEvent);
  if (data.extendLifetimePromises.length === 0) {
    data.extendLifetimeDeferred.resolve();
  }
  return data.extendLifetimeDeferred.promise;
}

class FetchEvent extends ExtendableEvent {
  constructor(type, eventInitDict) {
    super(type, eventInitDict);
    eventInitDict = eventInitDict ?? {};
    $(this).request = eventInitDict.request;
    $(this).preloadResponse = eventInitDict.preloadResponse ?? Promise.resolve();
    $(this).clientId = eventInitDict.clientId ?? '';
    $(this).resultingClientId = eventInitDict.resultingClientId ?? '';
    $(this).replacesClientId = eventInitDict.replacesClientId ?? '';
    $(this).handled = eventInitDict.handled ?? Promise.resolve();

    $(this).waitToRespondFlag = false;
    $(this).respondWithEnteredFlag = false;
    $(this).respondWithErrorFlag = false;
    $(this).potentialResponse = undefined;

    $(this).respondWithPromise;
  }

  get clientId() {
    const data = $(this);
    return data.clientId;
  }

  get preloadResponse() {
    const data = $(this);
    return data.preloadResponse;
  }

  get replacesClientId() {
    const data = $(this);
    return data.replacesClientId;
  }

  get resultingClientId() {
    const data = $(this);
    return data.resultingClientId;
  }

  get request() {
    const data = $(this);
    return data.request;
  }

  // https://w3c.github.io/ServiceWorker/#fetch-event-respondwith
  respondWith(ret) {
    const data = $(this);
    if (!data.dispatchFlag) {
      throw createInvalidStateError();
    }
    if (data.respondWithEnteredFlag) {
      throw createInvalidStateError();
    }
    ret = Promise.resolve(ret);
    this.waitUntil(ret);
    data.stopPropagationFlag = true;
    data.stopImmediatePropagationFlag = true;
    data.respondWithEnteredFlag = true;
    data.waitToRespondFlag = true;

    data.respondWithPromise = ret.then(
      response => {
        if (!(response instanceof Response)) {
          data.respondWithErrorFlag = true;
          return;
        }
        let _done = false;
        let newStream;
        if (response.body) {
          const reader = response.body.getReader();
          newStream = new ReadableStream({
            pull(controller) {
              if (_done) {
                return;
              }
              reader.read()
                .then(
                  ({ done, value }) => {
                    if (_done) {
                      return;
                    }
                    // TODO: detach arraybuffer;
                    if (done) {
                      controller.close();
                      _done = true;
                    } else {
                      controller.enqueue(value);
                    }
                  },
                  err => {
                    controller.error(err);
                    _done = true;
                  }
                );
            },
            cancel(reason) {
              return reader.cancel(reason);
            },
          });
        }
        data.potentialResponse = new Response(newStream, response);
        data.waitToRespondFlag = false;
      },
      err => {
        // TODO: handle error flag;
        data.respondWithErrorFlag = true;
        data.waitToRespondFlag = false;
        throw err;
      });
  }
}

const kBaggage = Symbol.for('aworker.fetch.baggage');
function createFetchEvent(readableStream, init) {
  // ignore body of GET/HEAD requests
  if (init.method == null || [ 'GET', 'HEAD' ].includes(init.method)) {
    readableStream = null;
  }
  const request = new Request(init.url ?? '', {
    method: init.method,
    headers: init.headers,
    body: readableStream,
  });
  const baggage = createBaggages(init.baggage);
  request[kBaggage] = baggage;

  const event = new FetchEvent('fetch', { request });
  return event;
}

function getFetchEventRespondWithPromise(fetchEvent) {
  return Promise.resolve($(fetchEvent).respondWithPromise).then(() => $(fetchEvent).potentialResponse);
}

wrapper.mod = {
  FetchEvent,
  createFetchEvent,
  ExtendableEvent,
  createExtendableEvent,

  getExtendableEventExtendedLifetimePromise,
  getFetchEventRespondWithPromise,
};
