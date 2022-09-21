'use strict';

// This file is inspired from https://github.com/mysticatea/event-target-shim/blob/master/src/lib/event-wrapper.ts

const { Event } = load('dom/event');

class EventWrapper extends Event {
  constructor(e) {
    super(e.type, {
      bubbles: e.bubbles,
      cancelable: e.cancelable,
      composed: e.composed,
    });

    if (e.cancelBubble) {
      super.stopPropagation();
    }

    if (e.defaultPrevented) {
      super.preventDefault();
    }

    Object.defineProperty(this, 'original', {
      value: e,
      enumerable: false,
      configurable: false,
      writable: false,
    });

    const keys = Object.keys(e);
    for (const key of keys) {
      if (!(key in this)) {
        const d = Object.getOwnPropertyDescriptor(e, key);
        Object.defineProperty(this, key, {
          get: () => {
            const { original } = this;
            const value = original[key];
            if (typeof value === 'function') {
              return value.bind(original);
            }
            return value;
          },
          set: value => {
            const { original } = this;
            original[key] = value;
          },
          configurable: d.configurable,
          enumerable: d.enumerable,
        });
      }
    }
  }

  stopPropagation() {
    super.stopPropagation();
    const { original } = this;
    if ('stopPropagation' in original) {
      original.stopPropagation();
    }
  }

  get cancelBubble() {
    return super.cancelBubble;
  }

  set cancelBubble(value) {
    super.cancelBubble = value;
    const { original } = this;
    if (original.cancelBubble) {
      original.cancelBubble = value;
    }
  }

  stopImmediatePropagation() {
    super.stopImmediatePropagation();
    const { original } = this;
    if ('stopImmediatePropagation' in original) {
      original.stopImmediatePropagation();
    }
  }

  get returnValue() {
    return super.returnValue;
  }

  set returnValue(value) {
    super.returnValue = value;
    const { original } = this;
    if ('returnValue' in original) {
      original.returnValue = value;
    }
  }

  preventDefault() {
    super.preventDefault();
    const { original } = this;
    if ('preventDefault' in original) {
      original.preventDefault();
    }
  }

  get timeStamp() {
    const { original } = this;
    if ('timeStamp' in original) {
      return original.timeStamp;
    }
    return super.timeStamp;
  }
}

EventWrapper.wrap = function wrap(e) {
  return new EventWrapper(e);
};

wrapper.mod = {
  EventWrapper,
};
