'use strict';

const process = loadBinding('process');
const { kPromiseRejectWithNoHandler, kPromiseHandlerAddedAfterReject } = loadBinding('errors');
const prepareStackTracePlain = load('source_maps/prepare_stack_trace_plain').prepareStackTrace;
const { trustedDispatchEvent, hasListeners } = load('dom/event_target');
const { getEventInternalData: $ } = load('dom/event');
const { createErrorEvent, createPromiseRejectionEvent } = load('dom/event_objects');
const { console } = load('console');

// TODO: trigger diag-reports
function fatalError(e, reason) {
  console.error(`${reason}:`, e);
  process.exit(1);
}

let prepareStackTraceCallback = prepareStackTracePlain;
function prepareStackTrace(globalThis, error, callSites) {
  return prepareStackTraceCallback(globalThis, error, callSites);
}

function setPrepareStackTraceCallback(fn) {
  prepareStackTraceCallback = fn;
}

const PromiseRejectionEvents = {
  [kPromiseRejectWithNoHandler]: 'unhandledrejection',
  [kPromiseHandlerAddedAfterReject]: 'rejectionhandled',
};
const promiseRejectionReasonWeakMap = new WeakMap();
let pendingPromiseEvents = new Map();
const processUnhandledRejection = self => {
  const _pendingPromiseEvents = pendingPromiseEvents;
  pendingPromiseEvents = new Map();
  for (let { event, reason, promise } of _pendingPromiseEvents.values()) {
    const eventName = PromiseRejectionEvents[event];
    if (eventName == null) {
      continue;
    }

    if (event === kPromiseRejectWithNoHandler) {
      promiseRejectionReasonWeakMap.set(promise, reason);
    }
    if (event === kPromiseHandlerAddedAfterReject) {
      reason = promiseRejectionReasonWeakMap.get(promise);
    }

    const _hasListeners = hasListeners(self, eventName);
    let canceled = false;
    if (_hasListeners) {
      const eventObject = createPromiseRejectionEvent(eventName, promise, reason);
      try {
        trustedDispatchEvent(self, eventObject);
      } catch (e) {
        return fatalError(e, 'Uncaught Exception on "unhandledrejection"');
      }
      canceled = $(eventObject).canceledFlag;
    }

    if (!canceled && event === kPromiseRejectWithNoHandler) {
      console.error('Uncaught (in promise)', reason);
      process.exit(1);
    }
  }
  return pendingPromiseEvents.size === 0;
};

let cleanupHooks = [];
function runCleanupHooks() {
  const hooks = cleanupHooks;
  cleanupHooks = [];
  for (const it of hooks.reverse()) {
    it();
  }
}

function addCleanupHook(fn) {
  cleanupHooks.push(fn);
}

/**
 * Mixin target must implement EventTarget interface.
 */
wrapper.mod = {
  addCleanupHook,
  runCleanupHooks,
  prepareStackTrace,
  setPrepareStackTraceCallback,
  emitUncaughtException(error) {
    const _hasListeners = hasListeners(this, 'error');
    if (!_hasListeners) {
      return false;
    }

    const event = createErrorEvent('error', error);
    try {
      trustedDispatchEvent(this, event);
    } catch (e) {
      return fatalError(e, 'Uncaught Exception on "error"');
    }
    if (!$(event).canceledFlag) {
      console.error('Uncaught', error);
    }
    return true;
  },
  emitPromiseRejection(event, promise, reason) {
    if (event === kPromiseRejectWithNoHandler) {
      pendingPromiseEvents.set(promise, {
        event,
        reason,
        promise,
      });
    }
    if (event === kPromiseHandlerAddedAfterReject) {
      if (pendingPromiseEvents.has(promise)) {
        pendingPromiseEvents.delete(promise);
      } else {
        pendingPromiseEvents.set(promise, {
          event, reason, promise,
        });
      }
    }
    return true;
  },
  processUnhandledRejection,
};
