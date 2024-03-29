'use strict';

const timers = loadBinding('timers');
const immediate = loadBinding('immediate');
const taskQueue = loadBinding('task_queue');
const { console } = load('console');

const TIMEOUT_MAX = 2 ** 31 - 1;

const DESTROY = Symbol('Timeout#destroy');
const SPIN = Symbol('Timeout#spin');
const TIMERS = new Map();

const IMMEDIATE_DESTROY = Symbol('Immediate#destroy');
const RUN = Symbol('Immediate#run');
const immediateMap = new Map();

let seq = 0;
let immediateSeq = 0;

// Refer to: https://github.com/nodejs/node/blob/cef1444/lib/internal/timers.js#L165-L174
function formatDelay(delay) {
  delay *= 1; // Convert to number or NaN
  if (!(delay >= 1 && delay <= TIMEOUT_MAX)) {
    if (delay > TIMEOUT_MAX) {
      console.warn(`${delay} does not fit into a 32-bit signed integer.\n` +
        'Timeout duration was set to 1.', 'TimeoutOverflowWarning');
    }

    delay = 1;
  }

  return delay;
}

class Timeout {
  #handle;

  constructor(group, loop, func, delay, ...args) {
    if (typeof func !== 'function') {
      throw new Error('Callback argument is invalid.');
    }

    this._spinning = false;
    this._destroyed = false;
    this._group = group;
    this._loop = loop;
    this._onTimeout = func;
    this._delay = formatDelay(delay);
    this._args = args;
  }

  #ontimeout = () => {
    try {
      this._onTimeout(...this._args);
    } finally {
      if (!this._loop) {
        this[DESTROY]();
      }
    }
  };

  [SPIN]() {
    if (this._spinning || this._destroyed) return;
    this._id = seq++;
    this.#handle = new timers.TimerWrap(this._delay, this._loop);
    this.#handle.ontimeout = this.#ontimeout;
    this._group.set(this._id, this);
    this._spinning = true;
    return this._id;
  }

  [DESTROY]() {
    if (this._destroyed) return;
    if (typeof this._id !== 'number') return;
    if (!this._group.has(this._id)) return;
    this._group.delete(this._id);
    this.#handle.remove();
    this.#handle = null;

    this._group = null;
    this._loop = false;
    this._onTimeout = null;
    this._delay = 0;
    this._args = null;
    this._spinning = false;
    this._destroyed = true;
  }

  toString() {
    return this._id === null ? 'null' : this._id.toString();
  }
}

class Immediate {
  #handle;
  constructor(callback, ...args) {
    if (typeof callback !== 'function') {
      throw new Error('Callback argument is invalid.');
    }

    this._args = args;
    this._destroyed = false;
    this._onimmediate = callback;
  }

  #onimmediate = () => {
    try {
      this._onimmediate(...this._args);
    } finally {
      this[IMMEDIATE_DESTROY]();
    }
  };

  [RUN]() {
    this._id = immediateSeq++;
    this.#handle = new immediate.ImmediateWrap();
    this.#handle.onimmediate = this.#onimmediate;
    immediateMap.set(this._id, this);
    return this._id;
  }

  [IMMEDIATE_DESTROY]() {
    if (this._destroyed) return;
    if (typeof this._id !== 'number') return;
    if (!immediateMap.has(this._id)) return;
    immediateMap.delete(this._id);
    this.#handle.remove();
    this.#handle = null;
    this._onimmediate = null;
    this._args = null;
    this._destroyed = true;
  }

  toString() {
    return this._id === null ? 'null' : this._id.toString();
  }
}

function runTimer(group, loop, func, delay, ...args) {
  const timeout = new Timeout(group, loop, func, delay, ...args);
  timeout[SPIN]();
  return timeout;
}

function runImmediate(func, ...args) {
  const immediate = new Immediate(func, ...args);
  immediate[RUN]();
  return immediate;
}

function clearTimer(group, id) {
  if (id instanceof Timeout) {
    if (id._group !== group) return false;
  } else {
    if (!group.has(id)) return false;
    id = group.get(id);
  }

  id[DESTROY]();
  return true;
}

function setTimeout(func, delay, ...args) {
  return runTimer(TIMERS, false, func, delay, ...args);
}

function setInterval(func, delay, ...args) {
  return runTimer(TIMERS, true, func, delay, ...args);
}

function setImmediate(func, ...args) {
  return runImmediate(func, ...args);
}

function clearTimeout(id) {
  clearTimer(TIMERS, id);
}

function clearInterval(id) {
  clearTimer(TIMERS, id);
}

function clearImmediate(id) {
  if (!id instanceof Immediate) {
    if (!immediateMap.has(id)) return false;
    id = immediateMap.get(id);
  }

  id[IMMEDIATE_DESTROY]();
  return true;
}

// Refer: https://developer.mozilla.org/en-US/docs/Web/API/WindowOrWorkerGlobalScope/queueMicrotask#When_queueMicrotask_isnt_available
function queueMicrotask(callback) {
  if (typeof callback !== 'function') {
    throw new TypeError('Expect a function on queueMicrotask');
  }
  taskQueue.enqueueMicrotask(callback);
}

wrapper.mod = {
  setTimeout,
  clearTimeout,

  setInterval,
  clearInterval,

  setImmediate,
  clearImmediate,

  queueMicrotask,
};
