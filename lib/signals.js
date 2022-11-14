'use strict';

const signals = loadBinding('signals');

class Signal {
  constructor(signal, func) {
    if (typeof func !== 'function') {
      throw new Error('Callback argument is invalid.');
    }

    this._signal = signal;
    this._onSignal = func;

    return new signals.SignalWrap(signal, func);
  }

}

wrapper.mod = Signal;
