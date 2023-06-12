'use strict';

// This file is inspired from https://github.com/mysticatea/abort-controller/blob/master/src/abort-controller.ts and
// is using WHATWG (https://dom.spec.whatwg.org/#interface-abortcontroller).

const { abortSignal, createAbortSignal } = load('dom/abort_signal');
const {
  private_symbols: {
    abort_controller_signal_symbol,
  },
} = loadBinding('constants');

class AbortController {
  constructor() {
    this[abort_controller_signal_symbol] = createAbortSignal();
  }

  get signal() {
    const signal = this[abort_controller_signal_symbol];
    if (signal === null) {
      throw new TypeError(`Expected 'this' to be an 'AbortController' object, but got ${
        this === null ? 'null' : typeof this
      }`);
    }
    return signal;
  }

  abort() {
    abortSignal(this.signal);
  }
}

Object.defineProperties(AbortController.prototype, {
  signal: { enumerable: true },
  abort: { enumerable: true },
});

Object.defineProperty(AbortController.prototype, Symbol.toStringTag, {
  configurable: true,
  value: 'AbortController',
});

wrapper.mod = { AbortController };
