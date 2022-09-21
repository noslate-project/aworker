'use strict';

const { WorkerGlobalScope, InternalWorkerGlobalScope } = load('global/worker_global_scope');
const { defineGlobalProperties } = load('global/utils');
const { ExtendableEvent, FetchEvent } = load('service_worker/event');

function ServiceWorkerGlobalScope() {
  throw new TypeError('Illegal constructor');
}
Object.setPrototypeOf(ServiceWorkerGlobalScope.prototype, WorkerGlobalScope.prototype);

// https://w3c.github.io/ServiceWorker/#serviceworkerglobalscope-interface
function InternalServiceWorkerGlobalScope() {
  const self = globalThis;
  InternalWorkerGlobalScope(self);

  Object.setPrototypeOf(self, ServiceWorkerGlobalScope.prototype);

  defineGlobalProperties(self, {
    ExtendableEvent,
    FetchEvent,
    ServiceWorkerGlobalScope,
  });

  return self;
}

wrapper.mod = {
  ServiceWorkerGlobalScope,
  InternalServiceWorkerGlobalScope,
};
