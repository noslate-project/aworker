'use strict';

// This file is inspired from https://github.com/mysticatea/abort-controller/blob/master/src/abort-controller.ts and
// is using WHATWG (https://dom.spec.whatwg.org/#interface-abortcontroller).

const { abortSignal, createAbortSignal } = load('dom/abort_signal');

const signals = new WeakMap();

class AbortController {
  constructor() {
    signals.set(this, createAbortSignal());
  }

  get signal() {
    const signal = signals.get(this);
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
