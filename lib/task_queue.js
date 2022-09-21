'use strict';

const { performMicrotaskCheckpoint } = loadBinding('task_queue');
const { processUnhandledRejection } = load('process/execution');
const debug = load('console/debuglog').debuglog('task_queue');

const storageList = [];
let executionResource = new (class RootExecutionResource {})();
const executionMap = new WeakMap();

class AsyncLocalStorage {
  #enabled;
  #resourceStoreMap;

  constructor() {
    this.#enabled = false;
    this.#resourceStoreMap = new WeakMap();
  }

  // Propagate the context from a parent resource to a child one
  static initHook(resource) {
    const currentResource = getExecutionResource();
    debug('propagate store from %o to %o', currentResource, resource);
    for (let i = 0; i < storageList.length; ++i) {
      const storage = storageList[i];
      if (!storage.#enabled) {
        continue;
      }
      // Value of currentResource is always a non null object
      const store = storage.#resourceStoreMap.get(currentResource);
      storage.#resourceStoreMap.set(resource, store);
    }
  }

  #enable() {
    if (!this.#enabled) {
      this.#enabled = true;
      storageList.push(this);
    }
  }

  disable() {
    if (!this.#enabled) {
      return;
    }
    this.#enabled = false;
    // If this.enabled, the instance must be in storageList
    storageList.splice(storageList.indexOf(this), 1);
  }

  run(store, callback, ...args) {
    // Avoid creation of an AsyncResource if store is already active
    if (Object.is(store, this.getStore())) {
      return Reflect.apply(callback, null, args);
    }

    this.#enable();

    const resource = getExecutionResource();
    const oldStore = this.#resourceStoreMap.get(resource);
    this.#resourceStoreMap.set(resource, store);

    try {
      return Reflect.apply(callback, null, args);
    } finally {
      this.#resourceStoreMap.set(resource, oldStore);
    }
  }

  getStore() {
    if (!this.#enabled) {
      return;
    }
    const resource = getExecutionResource();
    debug('get store on resource', resource);

    return this.#resourceStoreMap.get(resource);
  }
}

// eslint-disable-next-line no-unused-vars
function asyncInit(resource, triggerResource) {
  AsyncLocalStorage.initHook(resource);
  // TODO: schedule inspector async tasks;
}

function asyncBefore(resource) {
  executionMap.set(resource, executionResource);
  executionResource = resource;
  // TODO: schedule inspector async tasks;
}

function asyncAfter(resource) {
  if (resource !== executionResource) {
    return;
  }
  const parent = executionMap.get(resource, executionResource);
  executionResource = parent;
  // TODO: schedule inspector async tasks;
}

function getExecutionResource() {
  return executionResource;
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
  asyncWrapBeforeFunction: asyncBefore,
  asyncWrapAfterFunction: asyncAfter,

  promiseInitHook: asyncInit,
  promiseBeforeHook: asyncBefore,
  promiseAfterHook: asyncAfter,

  getExecutionResource,
  AsyncLocalStorage,
};
