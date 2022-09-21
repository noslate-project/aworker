'use strict';
/* global getNativeModule, getBinding */

const bindings = new Map();
const nativeModules = new Map();

// eslint-disable-next-line no-redeclare
function loadBinding(name) {
  const item = bindings.get(name);

  if (item) {
    return item;
  }

  let exports = Object.create(null);
  exports = getBinding(`${name}`, exports);

  bindings.set(name, exports);
  return exports;
}

// eslint-disable-next-line no-redeclare
function load(name) {
  const item = nativeModules.get(name);

  if (item) {
    return item.mod;
  }

  const wrapper = Object.create(null);
  wrapper.mod = Object.create(null);
  const func = getNativeModule(`${name}.js`);

  nativeModules.set(name, wrapper);
  func(wrapper, wrapper.mod, load, loadBinding);

  return wrapper.mod;
}

/** return value: loader */
return {
  load,
  loadBinding,
};
