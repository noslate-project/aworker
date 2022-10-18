'use strict';

const _ac = loadBinding('noslated_data_channel');
const { headersDataKey } = load('fetch/headers');
const { ReadableStream } = load('streams');
const {
  createFetchEvent,
  getFetchEventRespondWithPromise,
} = load('service_worker/event');
const { installFuture, dispatchActivateEvent } = load('service_worker/index');
const { trustedDispatchEvent } = load('dom/event_target');
const { TextEncoder } = load('encoding');
const { safeError, createDeferred } = load('utils');
const { debuglog, traceAsync } = load('console/debuglog');
const { trace } = loadBinding('tracing');
const {
  TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN,
  TRACE_EVENT_PHASE_NESTABLE_ASYNC_END,
} = loadBinding('constants').tracing;

const debug = debuglog('agent');
const kCategory = 'aworker,aworker.agent_channel';
const kCallCategory = 'aworker,aworker.agent_channel,aworker.agent_channel.call';
const empty = new Uint8Array(0);
let _traceEventId = 0;
const nextTraceEventId = () => {
  if (_traceEventId === Number.MAX_SAFE_INTEGER) {
    _traceEventId = 0;
  }
  return _traceEventId++;
};

const CanonicalCode = {
  OK: 0,
  INTERNAL_ERROR: 1,
  TIMEOUT: 2,
  NOT_IMPLEMENTED: 3,
  CONNECTION_RESET: 4,
  CLIENT_ERROR: 5,
};

const ResourcePutAction = {
  ACQUIRE_SH: 0,
  ACQUIRE_EX: 1,
  RELEASE: 2,
};

class AgentChannel {
  #events = new Map();
  #readableControllerMap = new Map();
  #readableStreamWeakMap = new WeakMap();
  #resourceWaitList = new Map();

  constructor() {
    _ac.setHandler(this.#agentEmit);
    _ac.setCallback(this.#agentCallback);
  }

  #agentEmit = (id, type, params) => {
    const handler = this.#handlerMap[type];
    if (handler) {
      handler(id, params);
    }
  }

  #handlerMap = {
    trigger: (id, params) => {
      const { method, sid, metadata, hasInputData, hasOutputData } = params;
      const globalScope = load('global/index');
      if (method === 'init') {
        installFuture
          .then(() => dispatchActivateEvent())
          .then(
            () => {
              const feedback = {
                status: 200,
                headers: [],
              };
              _ac.feedback(id, CanonicalCode.OK, feedback);
            },
            e => {
              _ac.feedback(id, CanonicalCode.INTERNAL_ERROR, safeError(e));
              throw e;
            });
        return;
      }
      if (method === 'invoke') {
        let readable;
        if (hasInputData) {
          readable = this.createReadableStream(sid);
        }
        installFuture
          .then(() => {
            const event = createFetchEvent(readable, metadata);
            trustedDispatchEvent(globalScope, event);
            const promise = getFetchEventRespondWithPromise(event);
            return promise;
          })
          .then(
            potentialResponse => {
              if (potentialResponse == null) {
                _ac.feedback(id, CanonicalCode.INTERNAL_ERROR, new TypeError('Event settled without response'));
                return;
              }
              const feedback = {
                status: potentialResponse.status,
                headers: potentialResponse.headers[headersDataKey],
              };
              _ac.feedback(id, CanonicalCode.OK, feedback);
              this.pipeStreamsToAgent(hasOutputData ? sid : null, potentialResponse.body);
            },
            e => {
              _ac.feedback(id, CanonicalCode.INTERNAL_ERROR, safeError(e));
              throw e;
            }
          );
        return;
      }
      _ac.feedback(id, CanonicalCode.INTERNAL_ERROR, new TypeError('Unrecognizable method'));
    },
    streamPush: (id, params) => {
      _ac.feedback(id, CanonicalCode.OK, {});
      const { sid, isEos, isError, data } = params;
      const controller = this.#readableControllerMap.get(sid);
      if (controller == null) {
        return;
      }
      switch (true) {
        case isError: {
          controller.error(new TypeError('Peer reset stream'));
          this.#readableControllerMap.delete(sid);
          _ac.unref();
          break;
        }
        case isEos: {
          controller.close();
          this.#readableControllerMap.delete(sid);
          _ac.unref();
          break;
        }
        default: {
          controller.enqueue(data);
        }
      }
    },
    resourceNotification: (id, params) => {
      debug('on resource notification', params);
      _ac.feedback(id, CanonicalCode.OK, {});
      const { token } = params;
      if (!this.#resourceWaitList.has(token)) {
        return;
      }
      const deferred = this.#resourceWaitList.get(token);
      this.#resourceWaitList.delete(token);
      deferred.resolve();
    },
  };

  #agentCallback = (id, error, body) => {
    // TODO(chengzhong.wcz): trigger uncaught exception and exit process from js
    const process = loadBinding('process');
    if (id instanceof Error) {
      process.emitUncaughtException(id);
      return;
    }

    if (!this.#events.has(id)) {
      return;
    }

    const ev = this.#events.get(id);
    if (error !== undefined) {
      ev.promise.reject(error);
    } else {
      ev.promise.resolve(body);
    }
    this.#events.delete(id);
  }

  call(type, params, timeout = 10_000) {
    const id = _ac[type](params, timeout);
    if (id == null) {
      throw new TypeError('agent call failed');
    }

    const deferred = createDeferred();
    deferred.promise = traceAsync(kCallCategory, type, id, deferred.promise);
    this.#events.set(id, {
      id,
      type,
      promise: deferred,
    });

    return deferred.promise;
  }

  /**
   * Create a ReadableStream from the given stream id.
   * The returned stream doesn't need to be manually closed as the stream will
   * be closed automatically on EOS (End of Stream) or error-ed and been clear
   * from resource table.
   *
   * @param {number} sid stream id to be opened to read
   */
  createReadableStream(sid) {
    const readable = new ReadableStream({
      start: controller => {
        this.#readableControllerMap.set(sid, controller);
      },
    });
    this.#readableStreamWeakMap.set(readable, sid);
    _ac.ref();
    return readable;
  }

  /**
   * Force close a readable stream.
   * @param {ReadableStream} readableStream -
   */
  closeReadableStream(readableStream) {
    const sid = this.#readableStreamWeakMap.get(readableStream);
    if (sid == null) {
      return;
    }
    if (this.#readableControllerMap.has(sid)) {
      const controller = this.#readableControllerMap.get(sid);
      controller.close();
      this.#readableControllerMap.delete(sid);
      _ac.unref();
    }
  }

  abortReadableStream(readableStream, reason) {
    const sid = this.#readableStreamWeakMap.get(readableStream);
    if (sid == null) {
      return;
    }
    if (this.#readableControllerMap.has(sid)) {
      const controller = this.#readableControllerMap.get(sid);
      controller.error(reason);
      this.#readableControllerMap.delete(sid);
      _ac.unref();
    }
  }

  pipeStreamsToAgent = async (sid, readableStream, reader) => {
    if (readableStream != null && reader == null) {
      reader = readableStream.getReader();
    }
    if (sid == null && readableStream != null) {
      // exhaust stream;
      while (true) {
        const { done } = await reader.read();
        if (done) {
          break;
        }
      }
      return;
    }
    if (readableStream == null) {
      this.streamPush(sid, /* isEos */true, /* chunk */empty, /* isError */false);
      return;
    }
    const encoder = new TextEncoder();
    try {
      while (true) {
        const { done, value } = await reader.read();
        if (done) {
          break;
        }
        let buffer;
        if (typeof value === 'string') {
          buffer = encoder.encode(value);
        } else if (value instanceof ArrayBuffer) {
          buffer = new Uint8Array(value);
        } else if (value instanceof Uint8Array) {
          buffer = value;
        } else if (!value) {
          // noop for undefined
        } else {
          throw new TypeError('unhandled type on stream read');
        }
        this.streamPush(sid, false, buffer);
      }
      this.streamPush(sid, true, empty);
    } catch (e) {
      this.streamPush(sid, /* isEos */true, /* chunk */empty, /* isError */true);
    }
  }

  streamPush(sid, isEos, bufferOrView, isError = false) {
    if (ArrayBuffer.isView(bufferOrView) && !(bufferOrView instanceof Uint8Array)) {
      bufferOrView = new Uint8Array(bufferOrView.buffer, bufferOrView.byteOffset, bufferOrView.byteLength);
    }
    if (bufferOrView instanceof ArrayBuffer) {
      bufferOrView = new Uint8Array(bufferOrView);
    }
    if (!(bufferOrView instanceof Uint8Array)) {
      throw new TypeError('expecting Uint8Array');
    }
    this.call('streamPush', {
      sid, isEos, isError,
      data: bufferOrView,
    });
  }

  // TODO(chengzhong.wcz): queue mode?
  async acquireResource(resourceId, exclusive) {
    const id = nextTraceEventId();
    trace(TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN, kCategory,
      'acquireResource', id,
      {
        resourceId,
        exclusive,
      });

    const { success_or_acquired, token } = await this.call('resourcePut', {
      resourceId,
      action: exclusive ? ResourcePutAction.ACQUIRE_EX : ResourcePutAction.ACQUIRE_SH,
      token: '',
    });

    debug('on acquired resource(%s, token: %s): %s', resourceId, token, success_or_acquired);
    if (!success_or_acquired) {
      const deferred = createDeferred();
      this.#resourceWaitList.set(token, deferred);
      trace(TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN, kCategory,
        'awaitAcquireResource', id,
        {
          resourceId,
          exclusive,
        });
      await deferred.promise;
      trace(TRACE_EVENT_PHASE_NESTABLE_ASYNC_END, kCategory,
        'awaitAcquireResource', id,
        {
          resourceId,
          exclusive,
        });
    }
    trace(TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN, kCategory,
      'resourceWork', id,
      {
        resourceId,
        exclusive,
      });
    return {
      release: () => {
        trace(TRACE_EVENT_PHASE_NESTABLE_ASYNC_END, kCategory,
          'resourceWork', id,
          {
            resourceId,
            exclusive,
          });
        this.call('resourcePut', {
          resourceId,
          action: ResourcePutAction.RELEASE,
          token,
        }).catch(e => {
          debug('unexpected error on release resource(%s, token: %s)', resourceId, token, e);
        }).finally(() => {
          trace(TRACE_EVENT_PHASE_NESTABLE_ASYNC_END, kCategory,
            'acquireResource', id,
            {
              resourceId,
              exclusive,
            });
        });
      },
    };
  }
}

wrapper.mod = new AgentChannel();
