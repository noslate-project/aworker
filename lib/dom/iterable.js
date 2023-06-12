'use strict';

const { requiredArguments } = load('utils');

function DomIterableMixin(dataSymbol) {
  const DomIterable = {
    * entries() {
      for (const entry of this[dataSymbol]) {
        yield entry;
      }
    },

    * keys() {
      for (const [ key ] of this[dataSymbol]) {
        yield key;
      }
    },

    * values() {
      for (const [ , value ] of this[dataSymbol]) {
        yield value;
      }
    },

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
    },

    * [Symbol.iterator]() {
      for (const entry of this[dataSymbol]) {
        yield entry;
      }
    },
  };

  return DomIterable;
}

wrapper.mod = {
  DomIterableMixin,
};
