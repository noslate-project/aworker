'use strict';

const { requiredArguments } = load('utils');

function DomIterableMixin(Base, dataSymbol) {
  const DomIterable = class extends Base {
    * entries() {
      for (const entry of this[dataSymbol]) {
        yield entry;
      }
    }

    * keys() {
      for (const [ key ] of this[dataSymbol]) {
        yield key;
      }
    }

    * values() {
      for (const [ , value ] of this[dataSymbol]) {
        yield value;
      }
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
      for (const [ key, value ] of this[dataSymbol]) {
        callbackfn(value, key, this);
      }
    }

    * [Symbol.iterator]() {
      for (const entry of this[dataSymbol]) {
        yield entry;
      }
    }
  };

  // we want the Base class name to be the name of the class.
  Object.defineProperty(DomIterable, 'name', {
    value: Base.name,
    configurable: true,
  });

  return DomIterable;
}

wrapper.mod = {
  DomIterableMixin,
};
