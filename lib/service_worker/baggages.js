'use strict';

const { requiredArguments } = load('utils');

function validateName(name) {
  if (name === '') {
    throw new TypeError(`${name} is not a legal name`);
  }
}

const kBaggagesDataSymbol = Symbol('baggages.data');
class Baggages {
  constructor() {
    throw new Error('Invalid construction');
  }

  delete(name) {
    requiredArguments('Baggages.delete', arguments.length, 1);
    name = String(name).toLowerCase();
    this[kBaggagesDataSymbol].delete(name);
  }

  get(name) {
    requiredArguments('Baggages.get', arguments.length, 1);
    name = String(name).toLowerCase();
    return this[kBaggagesDataSymbol].get(name);
  }

  has(name) {
    requiredArguments('Baggages.has', arguments.length, 1);
    name = String(name).toLowerCase();
    return this[kBaggagesDataSymbol].has(name);
  }

  set(name, value) {
    requiredArguments('Baggages.set', arguments.length, 2);
    name = String(name).toLowerCase();
    value = String(value).trim();
    validateName(name);
    return this[kBaggagesDataSymbol].set(name, value);
  }

  get [Symbol.toStringTag]() {
    return 'Baggages';
  }

  entries() {
    return this[kBaggagesDataSymbol].entries();
  }

  keys() {
    return this[kBaggagesDataSymbol].keys();
  }

  values() {
    return this[kBaggagesDataSymbol].values();
  }

  forEach(
    callbackfn,
    thisArg
  ) {
    requiredArguments(
      `${this.constructor.name}.forEach`,
      arguments.length,
      1
    );
    callbackfn = callbackfn.bind(
      thisArg == null ? globalThis : Object(thisArg)
    );
    for (const [ key, value ] of this[kBaggagesDataSymbol].entries()) {
      callbackfn(value, key, this);
    }
  }

  [Symbol.iterator]() {
    return this[kBaggagesDataSymbol].entries();
  }
}

function InternalBaggages(init) {
  if (Array.isArray(init)) {
    this[kBaggagesDataSymbol] = new Map(init);
  } else if (init != null && typeof init === 'object') {
    this[kBaggagesDataSymbol] = new Map(Object.entries(init));
  } else {
    this[kBaggagesDataSymbol] = new Map();
  }
}

function createBaggages(init) {
  return Reflect.construct(InternalBaggages, [ init ], Baggages);
}

wrapper.mod = {
  createBaggages,
  Baggages,
};
