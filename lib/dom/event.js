'use strict';

// This file is inspired from https://github.com/mysticatea/event-target-shim/blob/master/src/lib/event.ts and
// is using WHATWG (https://dom.spec.whatwg.org/#events).

const { getTimeOriginRelativeTimeStamp } = loadBinding('perf');
const utils = load('utils');

const SET_CANCEL_FLAG = Symbol('Event#setCancelFlag');
const INTERNAL_DATA = Symbol('Event#internalData');

function $(obj) {
  return obj[INTERNAL_DATA];
}

const PHASES = [ 'NONE', 'CAPTURING_PHASE', 'AT_TARGET', 'BUBBLING_PHASE' ];

// wpt: Event-isTrusted.any.js -> `isTrusted` getter in two Event objects'
//      descriptor should be strictly equal
//
// https://github.com/web-platform-tests/wpt/blob/master/dom/events/Event-isTrusted.any.js#L10
function isTrustedGetter() {
  return $(this).isTrusted;
}

function isEvent(it) {
  return utils.hasOwn(it, INTERNAL_DATA);
}

/**
 * WHATWG Event Class
 * @refs: https://dom.spec.whatwg.org/#interface-event
 */
class Event {
  constructor(type, eventInitDict = { bubbles: false, cancelable: false, composed: false }) {
    utils.requiredArguments('Event.constructor', arguments.length, 1);

    eventInitDict = eventInitDict || {};
    this[INTERNAL_DATA] = {
      type,
      bubbles: !!eventInitDict.bubbles,
      cancelable: !!eventInitDict.cancelable,
      composed: !!eventInitDict.composed,

      target: null,
      currentTarget: null,
      stopPropagationFlag: false,
      stopImmediatePropagationFlag: false,
      canceledFlag: false,
      inPassiveListenerFlag: false,
      dispatchFlag: false,
      timeStamp: getTimeOriginRelativeTimeStamp(),
      isTrusted: false,
    };

    Object.defineProperties(this, {
      isTrusted: {
        enumerable: true,
        configurable: false,
        get: isTrustedGetter,
      },
    });
  }

  get type() {
    return $(this).type;
  }

  get target() {
    return $(this).target;
  }

  get srcElement() {
    return this.target;
  }

  get currentTarget() {
    return $(this).currentTarget;
  }

  get composedPath() {
    const { currentTarget } = $(this);
    return currentTarget ? [ currentTarget ] : [];
  }

  get eventPhase() {
    return $(this).dispatchFlag ? this.AT_TARGET : this.NONE;
  }

  get cancelBubble() {
    return $(this).stopPropagationFlag;
  }

  set cancelBubble(value) {
    // The cancelBubble attribute's setter, when invoked, must set this's stop
    // propagation flag if the given value is true, and do nothing otherwise.
    if (value === true) {
      this.stopPropagation();
    }
  }

  get bubbles() {
    return $(this).bubbles;
  }

  get cancelable() {
    return $(this).cancelable;
  }

  get returnValue() {
    return !$(this).canceledFlag;
  }

  set returnValue(value) {
    if (value === false) {
      this[SET_CANCEL_FLAG]();
    }
  }

  get defaultPrevented() {
    return $(this).canceledFlag;
  }

  get composed() {
    return $(this).composed;
  }

  get timeStamp() {
    return $(this).timeStamp;
  }

  stopPropagation() {
    $(this).stopPropagationFlag = true;
  }

  stopImmediatePropagation() {
    const data = $(this);
    data.stopPropagationFlag = data.stopImmediatePropagation = true;
  }

  preventDefault() {
    this[SET_CANCEL_FLAG]();
  }

  // deprecated
  initEvent(type, bubbles = false, cancelable = false) {
    const data = $(this);
    if (data.dispatchFlag) {
      return;
    }

    this[INTERNAL_DATA] = {
      ...data,
      type,
      bubbles: !!bubbles,
      cancelable: !!cancelable,

      target: null,
      currentTarget: null,
      stopPropagationFlag: false,
      stopImmediatePropagationFlag: false,
      canceledFlag: false,
    };
  }

  [SET_CANCEL_FLAG]() {
    const data = $(this);
    if (data.inPassiveListenerFlag || !data.cancelable) {
      return;
    }

    data.canceledFlag = true;
  }
}

for (let i = 0; i < PHASES.length; i++) {
  const idx = i;
  Event[PHASES[idx]] = idx;
  Object.defineProperty(Event.prototype, PHASES[idx], {
    enumerable: true,
    writable: false,
    configurable: false,
    value: idx,
  });
}

/**
 * WHATWG CustomEvent Class
 * @refs: https://dom.spec.whatwg.org/#customevent
 */
class CustomEvent extends Event {
  constructor(type, eventInitDict = { detail: null }) {
    utils.requiredArguments('CustomEvent.constructor', arguments.length, 1);
    super(type, eventInitDict);
    eventInitDict = eventInitDict || {};
    $(this).detail = eventInitDict.detail;
  }

  get detail() {
    return $(this).detail;
  }

  // deprecated
  initCustomEvent(type, bubbles = false, cancelable = false, detail = null) {
    const data = $(this);
    if (data.dispatchFlag) {
      return;
    }

    this[INTERNAL_DATA] = {
      ...data,
      type,
      detail,
      bubbles: !!bubbles,
      cancelable: !!cancelable,

      target: null,
      currentTarget: null,
      stopPropagationFlag: false,
      stopImmediatePropagationFlag: false,
      canceledFlag: false,
    };
  }
}

for (let i = 0; i < PHASES.length; i++) {
  CustomEvent[PHASES[i]] = i;
}

mod.Event = Event;
mod.CustomEvent = CustomEvent;
mod.isEvent = isEvent;
mod.getEventInternalData = $;
