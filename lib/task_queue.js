'use strict';

const { performMicrotaskCheckpoint } = loadBinding('task_queue');
const { processUnhandledRejection } = load('process/execution');

const kRootExecutionResource = new (class RootExecutionResource {})();
const executionResourceStack = [];
const resourceMapping = new WeakMap();

class AsyncLocalStorage {
  // Propagate the context from a parent resource to a child one
  static initHook(resource) {
    const currentResource = getExecutionResource();
    const mapping = resourceMapping.get(currentResource);
    resourceMapping.set(resource, mapping);
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
    const globalMapping = resourceMapping.get(resource);
    const snapshot = new Map(globalMapping?.entries());
    snapshot.set(this, store);
    resourceMapping.set(resource, snapshot);

    try {
      return Reflect.apply(callback, null, args);
    } finally {
      resourceMapping.set(resource, globalMapping);
    }
  }

  getStore() {
    const resource = getExecutionResource();
    const globalMapping = resourceMapping.get(resource);
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
  executionResourceStack.push(resource);
  // TODO: schedule inspector async tasks;
}

function asyncAfter() {
  executionResourceStack.pop();
  // TODO: schedule inspector async tasks;
}

function callbackTrampoline(callback, ...args) {
  asyncBefore(this);

  try {
    return callback.apply(this, args);
  } finally {
    asyncAfter();
  }
}

function getExecutionResource() {
  if (executionResourceStack.length === 0) {
    return kRootExecutionResource;
  }
  return executionResourceStack[executionResourceStack.length - 1];
}

/**
 * Mixin target must implement EventTarget interface.
 */
wrapper.mod = {
  tickTaskQueue() {
    performMicrotaskCheckpoint();
    return processUnhandledRejection(this);
  },
  asyncWrapInitFunction: asyncInit,
  callbackTrampolineFunction: callbackTrampoline,

  promiseInitHook: asyncInit,
  promiseBeforeHook: asyncBefore,
  promiseAfterHook: asyncAfter,

  getExecutionResource,
  AsyncLocalStorage,
};
