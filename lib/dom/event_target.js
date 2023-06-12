'use strict';

// This file is inspired from https://github.com/mysticatea/event-target-shim/blob/master/src/lib/event-target.ts
// is using WHATWG (https://dom.spec.whatwg.org/#interface-eventtarget).

const { Event, getEventInternalData } = load('dom/event');
const { EventWrapper } = load('dom/event_wrapper');
const {
  private_symbols: {
    event_target_private_symbol,
  },
} = loadBinding('constants');

const LISTEN_FLAGS = {
  CAPTURE: 0x01,
  PASSIVE: 0x02,
  ONCE: 0x04,
  REMOVED: 0x08,
};

function assertCallback(callback) {
  if (typeof callback === 'function' || typeof callback === 'object') {
    return;
  }

  throw new TypeError('The \'callback\' argument must be a function or an object that has \'handleEvent\' method');
}

function normalizeAddOptions(type, callback, options) {
  assertCallback(callback);

  if (typeof options === 'object' && options !== null) {
    return {
      type: String(type),
      callback,
      capture: !!options.capture,
      passive: !!options.passive,
      once: !!options.once,
      signal: options.signal ?? null,
    };
  }

  return {
    type: String(type),
    callback,
    capture: !!options,

    passive: false,
    once: false,
    signal: null,
  };
}

function normalizeRemoveOptions(type, callback, options) {
  assertCallback(callback);

  if (typeof options === 'object' && options !== null) {
    return {
      type: String(type),
      callback,
      capture: !!options.capture,
    };
  }

  return {
    type: String(type),
    callback,
    capture: !!options,
  };
}

/** @typedef {{list: Function[], attrListener: Function, attrCallback: Function, traversing: bool }} EventTargetSafeListeners */
function EventTarget() {
  /** @type {Map<string, EventTargetSafeListeners>} */
  const map = new Map();
  this[event_target_private_symbol] = map;
}

// refs: https://dom.spec.whatwg.org/#dom-eventtarget-addeventlistener
EventTarget.prototype.addEventListener = function addEventListener(_type, _callback, options) {
  // 1.1. Let capture, passive, and once be the result of flattening more options.
  const {
    callback,
    capture,
    once,
    passive,
    signal,
    type,
  } = normalizeAddOptions(_type, _callback, options);
  const listenerList = getSafeListenersForTarget(this, type);

  // 2.1. If eventTarget is a ServiceWorkerGlobalScope object, its service
  //      worker's script resource's has ever been evaluated flag is set, and
  //      listener's type matches the type attribute value of any of the
  //      service worker events, then report a warning to the console that
  //      this might not give the expected results.

  // 2.2. If signal is not null and its aborted flag is set, then return.
  if (signal?.aborted) {
    return;
  }

  // 2.3. If listener's callback is null, then return.
  if (callback === null) {
    return;
  }

  // 2.4. If eventTarget's event listener list does not contain an event
  //      listener whose type is listener's type, callback is listener's
  //      callback, and capture is listener's capture, then append listener to
  //      eventTarget's event listener list.
  for (const lstnr of listenerList.list) {
    if (lstnr.callback === callback && !!(lstnr.flags & LISTEN_FLAGS.CAPTURE) === capture) {
      return;
    }
  }

  let signalListener = null;
  if (signal) {
    signalListener = () => {
      this.removeEventListener(type, callback, { capture });
    };
  }

  const toPush = {
    callback,
    flags: (capture ? LISTEN_FLAGS.CAPTURE : 0) |
      (passive ? LISTEN_FLAGS.PASSIVE : 0) |
      (once ? LISTEN_FLAGS.ONCE : 0),
    signal,
    signalListener,
  };

  if (listenerList.traversing) {
    listenerList.traversing = false;
    listenerList.list = [ ...listenerList.list, toPush ];
  } else {
    listenerList.list.push(toPush);
  }

  // 2.5. If listener's signal is not null, then add the following abort steps
  //      to it:
  // 2.5.1. Remove an event listener with eventTarget and listener.
  if (signal) {
    signal.addEventListener('abort', signalListener);
  }
};

EventTarget.prototype.removeEventListener = function removeEventListener(_type, _callback, options) {
  // 1.1. Let capture be the result of flattening options.
  const {
    type,
    callback,
    capture,
  } = normalizeRemoveOptions(_type, _callback, options);
  const listenerList = getSafeListenersForTarget(this, type);
  if (!listenerList || !listenerList.list.length) {
    return;
  }

  // 2.1. If eventTarget is a ServiceWorkerGlobalScope object and its service
  //      worker's set of event types to handle contains type, then report a
  //      warning to the console that this might not give the expected
  //      results.

  if (callback === null) {
    return;
  }

  // 2.2. Set listener's removed to true and remove listener from
  //      eventTarget's event listener list.
  for (let i = 0; i < listenerList.list.length; i++) {
    const lstnr = listenerList.list[i];

    if (lstnr.callback === callback && !!(lstnr.flags & LISTEN_FLAGS.CAPTURE) === capture) {
      // Set listener's removed to true
      lstnr.flags |= LISTEN_FLAGS.REMOVED;

      // Remove signal listner & listener
      if (lstnr.signal) {
        lstnr.signal.removeEventListener('abort', lstnr.signalListener);
      }

      if (listenerList.traversing) {
        listenerList.traversing = false;
        listenerList.list = listenerList.list.filter((_, idx) => i !== idx);
      } else {
        listenerList.list.splice(i, 1);
      }
    }
  }
};

EventTarget.prototype.dispatchEvent = function dispatchEvent(e) {
  if (!e || !e.type) {
    throw new TypeError('invalid event object');
  }

  const event = (e instanceof Event) ? e : EventWrapper.wrap(e);
  return doDispatchEvent(this, event, false);
};

function trustedDispatchEvent(target, e) {
  if (!e || !e.type) {
    throw new TypeError('invalid event object');
  }

  const event = (e instanceof Event) ? e : EventWrapper.wrap(e);
  return doDispatchEvent(target, event, true);
}

function getSafeListenersForTarget(target, type) {
  const listenerMap = target[event_target_private_symbol];
  if (listenerMap == null) {
    throw new TypeError('Illegal invocation');
  }
  let listenerList = listenerMap.get(type);
  if (listenerList == null) {
    listenerList = { list: [], attrListener: null, attrCallback: null, traversing: false };
    listenerMap.set(type, listenerList);
  }
  return listenerList;
}

function setAttributeListener(target, type, callback) {
  const item = getSafeListenersForTarget(target, type);
  item.attrCallback = callback;

  if (typeof callback === 'function' || (typeof callback === 'object' && callback !== null)) {
    if (item.attrListener == null) {
      item.attrListener = event => {
        if (item.attrCallback) {
          item.attrCallback.call(target, event);
        }
      };
      target.addEventListener(type, item.attrListener);
    }
  } else {
    if (item.attrListener) {
      target.removeEventListener(type, item.attrListener);
      item.attrListener = null;
    }
  }
}

function getAttributeListener(target, type) {
  const listeners = target[event_target_private_symbol];
  if (listeners == null) {
    throw new TypeError('');
  }
  const item = listeners.get(type);
  return item?.attrCallback;
}

function doDispatchEvent(target, event, isTrusted) {
  const listeners = getSafeListenersForTarget(target, event.type);
  if (!listeners || !listeners.list.length) {
    return true;
  }

  const eventData = getEventInternalData(event);
  if (eventData.dispatchFlag) {
    throw new Error('This event has been in dispatching.');
  }
  eventData.isTrusted = isTrusted;

  eventData.dispatchFlag = true;
  eventData.target = eventData.currentTarget = target;

  if (!eventData.stopPropagationFlag) {
    let { list } = listeners;
    listeners.traversing = true;

    for (let i = 0; i < list.length; i++) {
      const listener = list[i];

      if ((listener.flags & LISTEN_FLAGS.REMOVED) === LISTEN_FLAGS.REMOVED) {
        continue;
      }

      // remove if once
      if ((listener.flags & LISTEN_FLAGS.ONCE) === LISTEN_FLAGS.ONCE) {
        listener.flags |= LISTEN_FLAGS.REMOVED;
      }

      eventData.inPassiveListenerFlag = ((listener.flags & LISTEN_FLAGS.PASSIVE) === LISTEN_FLAGS.PASSIVE);

      // callback
      const { callback } = listener;
      if (typeof callback === 'function') {
        callback.call(target, event);
      } else if (typeof callback.handleEvent === 'function') {
        callback.handleEvent(event);
      }
      eventData.inPassiveListenerFlag = false;
    }

    listeners.traversing = false;

    // real remove all removed
    list = listeners.list;
    for (let i = 0; i < list.length; i++) {
      const listener = list[i];

      if (!(listener.flags & LISTEN_FLAGS.REMOVED)) {
        continue;
      }

      list.splice(i, 1);
      i--;
    }
  }

  eventData.target = null;
  eventData.currentTarget = null;
  eventData.stopImmediatePropagationFlag = false;
  eventData.stopPropagationFlag = false;
  eventData.dispatchFlag = false;

  return !eventData.canceledFlag;
}

function hasListeners(target, type) {
  const listeners = target[event_target_private_symbol];
  if (listeners == null) {
    return false;
  }

  const specializedListeners = listeners.get(type);
  if (!specializedListeners || !specializedListeners.list.length) {
    return false;
  }

  return true;
}

mod.EventTarget = EventTarget;
mod.trustedDispatchEvent = trustedDispatchEvent;
mod.hasListeners = hasListeners;
mod.setAttributeListener = setAttributeListener;
mod.getAttributeListener = getAttributeListener;
