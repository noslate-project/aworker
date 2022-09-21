'use strict';

const { console } = load('console');

mod.defineGlobalProperty = function defineGlobalProperty(globalScope, name, pro) {
  Object.defineProperty(globalScope, name, {
    value: pro,
    enumerable: false,
    configurable: true,
    writable: true,
  });
};

mod.defineGlobalProperties = function defineGlobalProperties(globalScope, properties) {
  const p = {};
  for (const key of Object.getOwnPropertyNames(properties)) {
    p[key] = {
      value: properties[key],
      enumerable: false,
      configurable: true,
      writable: true,
    };
  }

  Object.defineProperties(globalScope, p);
};

mod.defineGlobalLazyProperties = function defineGlobalLazyProperties(globalScope, properties) {
  const p = {};
  for (const key of Object.getOwnPropertyNames(properties)) {
    p[key] = {
      enumerable: true,
      configurable: false,
      get: genLazyGetter(key, properties[key]),
    };
  }

  Object.defineProperties(globalScope, p);
};

function genLazyGetter(key, info) {
  let m = null;
  return () => {
    if (m) return m;
    m = load(info.path);
    m = info.property ? m[info.property] : m;

    if (info.message) {
      console.warn(info.message);
    }

    return m;
  };
}
