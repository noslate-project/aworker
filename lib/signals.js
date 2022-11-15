'use strict';

const signals = loadBinding('signals');

class Signal {
  constructor(signal) {
    this._signal = signal;
    this._handle = new signals.SignalWrap(signal);
  }
}

wrapper.mod = Signal;
