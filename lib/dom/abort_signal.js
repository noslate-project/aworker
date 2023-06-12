'use strict';

// This file is inspired from https://github.com/mysticatea/abort-controller/blob/master/src/abort-signal.ts and
// is using WHATWG (https://dom.spec.whatwg.org/#interface-AbortSignal).

const { EventTarget, trustedDispatchEvent, setAttributeListener, getAttributeListener } = load('dom/event_target');
const {
  private_symbols: {
    abort_signal_aborted_symbol,
  },
} = loadBinding('constants');

class AbortSignal extends EventTarget {
  constructor() {
    throw new Error('Illegal constructor');
  }

  get aborted() {
    const aborted = this[abort_signal_aborted_symbol];

    if (typeof aborted !== 'boolean') {
      throw new TypeError(`Expected 'this' to be an 'AbortSignal' object, but got ${
        this === null ? 'null' : typeof this
      }`);
    }

    return aborted;
  }
}

Object.defineProperty(AbortSignal.prototype, 'aborted', { enumerable: true });

// legacy
Object.defineProperty(AbortSignal.prototype, 'onabort', {
  enumerable: true,
  configurable: true,
  get() {
    return getAttributeListener(this, 'abort');
  },
  set(callback) {
    setAttributeListener(this, 'abort', callback);
  },
});

Object.defineProperty(AbortSignal.prototype, Symbol.toStringTag, {
  configurable: true,
  value: 'AbortSignal',
});

function createAbortSignal() {
  const signal = Object.create(AbortSignal.prototype);
  EventTarget.call(signal);
  signal[abort_signal_aborted_symbol] = false;
  return signal;
}

function abortSignal(signal) {
  if (signal[abort_signal_aborted_symbol] !== false) {
    return;
  }

  signal[abort_signal_aborted_symbol] = true;
  trustedDispatchEvent(signal, { type: 'abort' });
}

function isAbortSignal(it) {
  return typeof it === 'object' && it !== null && abort_signal_aborted_symbol in it;
}

function createFollowingSignal(signal) {
  const followingSignal = createAbortSignal();
  if (signal == null || !isAbortSignal(signal)) {
    return followingSignal;
  }
  if (signal.aborted) {
    abortSignal(followingSignal);
    return followingSignal;
  }
  signal.addEventListener('abort', () => {
    abortSignal(followingSignal);
  });
  return followingSignal;
}

wrapper.mod = {
  AbortSignal,
  abortSignal,
  createAbortSignal,
  createFollowingSignal,
};
