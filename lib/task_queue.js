'use strict';

const { performMicrotaskCheckpoint } = loadBinding('task_queue');
const {
  private_symbols: {
    async_context_mapping_symbol,
  },
} = loadBinding('constants');
const { processUnhandledRejection } = load('process/execution');

const kRootExecutionResource = new (class RootExecutionResource {})();
let executionResourceStackIdx = 0;
const kExecutionResourceStackMaxLength = 1024;
const executionResourceStack = new Array(kExecutionResourceStackMaxLength);

class AsyncLocalStorage {
  // Propagate the context from a parent resource to a child one
  static initHook(resource) {
    const currentResource = getExecutionResource();
    const mapping = currentResource[async_context_mapping_symbol];
    resource[async_context_mapping_symbol] = mapping;
  }

  disable() {
    /** no-op */
  }

  run(store, callback, ...args) {
    // Avoid creation of an AsyncResource if store is already active
    if (Object.is(store, this.getStore())) {
      return Reflect.apply(callback, null, args);
    }

    const resource = getExecutionResource();
    const globalMapping = resource[async_context_mapping_symbol];
    const snapshot = new Map(globalMapping?.entries());
    snapshot.set(this, store);
    resource[async_context_mapping_symbol] = snapshot;

    try {
      return Reflect.apply(callback, null, args);
    } finally {
      resource[async_context_mapping_symbol] = globalMapping;
    }
  }

  getStore() {
    const resource = getExecutionResource();
    const globalMapping = resource[async_context_mapping_symbol];
    if (globalMapping == null) {
      return undefined;
    }
    return globalMapping.get(this);
  }
}

function asyncInit(resource) {
  AsyncLocalStorage.initHook(resource);
  // TODO: schedule inspector async tasks;
}

function asyncBefore(resource) {
  if (executionResourceStackIdx >= kExecutionResourceStackMaxLength) {
    return;
  }
  executionResourceStack[executionResourceStackIdx++] = resource;
  // TODO: schedule inspector async tasks;
}

function asyncAfter() {
  executionResourceStack[--executionResourceStackIdx] = undefined;
  // TODO: schedule inspector async tasks;
}

function callbackTrampoline(callback, ...args) {
  asyncBefore(this);

  try {
    return callback.apply(this, args);
  } finally {
    asyncAfter();

    tickTaskQueue();
  }
}

function getExecutionResource() {
  if (executionResourceStackIdx === 0) {
    return kRootExecutionResource;
  }
  return executionResourceStack[executionResourceStackIdx - 1];
}

function tickTaskQueue() {
  let handled = false;
  do {
    performMicrotaskCheckpoint();
    handled = processUnhandledRejection(globalThis);
  } while (!handled);
}

/**
 * Mixin target must implement EventTarget interface.
 */
wrapper.mod = {
  tickTaskQueue,
  asyncWrapInitFunction: asyncInit,
  callbackTrampolineFunction: callbackTrampoline,

  promiseInitHook: asyncInit,
  promiseBeforeHook: asyncBefore,
  promiseAfterHook: asyncAfter,

  getExecutionResource,
  AsyncLocalStorage,
};
