'use strict';

const { InternalServiceWorkerGlobalScope } = load('global/service_worker_global_scope');

const globalScope = InternalServiceWorkerGlobalScope();

wrapper.mod = globalScope;
