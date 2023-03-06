'use strict';

const { Event, CustomEvent } = load('dom/event');
const { EventTarget } = load('dom/event_target');
const { ErrorEvent } = load('dom/event_objects');
const { AbortSignal } = load('dom/abort_signal');
const { AbortController } = load('dom/abort_controller');
const { defineGlobalProperties } = load('global/utils');

const { atob, btoa } = load('bytes');
const { Cache, CacheStorage } = load('cache');
const { console } = load('console');
const { DOMException } = load('dom/exception');
const { Blob } = load('dom/file');
const { Location } = load('dom/location');
const {
  TextDecoder,
  TextEncoder,
} = load('encoding');
const { fetch } = load('fetch/fetch');
const { FormData } = load('fetch/form');
const { Request, Response } = load('fetch/body');
const { Headers } = load('fetch/headers');
const { performance, Performance } = load('performance/performance');
const { PerformanceEntry } = load('performance/performance_entry');
const { PerformanceMark, PerformanceMeasure } = load('performance/user_timing');
const { PerformanceObserver, PerformanceObserverEntryList } = load('performance/observer');
const {
  navigator,
} = load('navigator');
const {
  ReadableStream,
  WritableStream,
  ReadableStreamDefaultReader,
  WritableStreamDefaultWriter,
} = load('streams');
const {
  setTimeout,
  setInterval,
  clearTimeout,
  clearInterval,
  queueMicrotask,
} = load('timer');
const {
  URL,
  URLSearchParams,
} = load('url');

const {
  AworkerNamespace,
  InternalAworkerNamespace,
} = load('global/aworker_namespace');

/**
 * See:
 * - WorkerGlobalScope, https://html.spec.whatwg.org/multipage/workers.html#the-workerglobalscope-common-interface
 * - WindowOrWorkerGlobalScope, https://html.spec.whatwg.org/multipage/webappapis.html#windoworworkerglobalscope-mixin
 */
function WorkerGlobalScope() {
  throw new TypeError('Illegal constructor');
}
Object.setPrototypeOf(WorkerGlobalScope.prototype, EventTarget.prototype);

function InternalWorkerGlobalScope(self) {
  // EventTarget Mixin;
  EventTarget.apply(self);
  const methods = [
    'addEventListener',
    'removeEventListener',
    'dispatchEvent',
  ];
  for (const m of methods) {
    self[m] = EventTarget.prototype[m].bind(self);
  }

  const caches = new CacheStorage();
  const aworker = new InternalAworkerNamespace();

  // WorkerGlobalScope
  defineGlobalProperties(self, {
    self,

    atob,
    btoa,
    caches,
    console,
    fetch,
    clearInterval,
    clearTimeout,
    navigator,
    performance,
    queueMicrotask,
    setInterval,
    setTimeout,

    AbortSignal,
    AbortController,
    Blob,
    Cache,
    CacheStorage,
    CustomEvent,
    DOMException,
    ErrorEvent,
    Event,
    EventTarget,
    FormData,
    Headers,
    Location,
    Performance,
    PerformanceEntry,
    PerformanceMark,
    PerformanceMeasure,
    PerformanceObserver,
    PerformanceObserverEntryList,
    ReadableStream,
    ReadableStreamDefaultReader,
    Request,
    Response,
    TextDecoder,
    TextEncoder,
    URL,
    URLSearchParams,
    WorkerGlobalScope,
    WritableStream,
    WritableStreamDefaultWriter,

    aworker,
    AworkerNamespace,
  });

  return self;
}

wrapper.mod = {
  WorkerGlobalScope,
  InternalWorkerGlobalScope,
};
