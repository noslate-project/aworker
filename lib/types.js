'use strict';

const native = loadBinding('types');
const {
  isArrayBufferDetached,
  detachArrayBuffer,
} = native;
const {
  toUSVString: _toUSVString,
} = loadBinding('bytes');

const ReflectApply = Reflect.apply;

const AsyncIteratorPrototype = Object
// eslint-disable-next-line no-empty-function
  .getPrototypeOf(Object.getPrototypeOf(async function* () {}).prototype);

// Cached to make sure no userland code can tamper with it.
const isArrayBufferView = ArrayBuffer.isView;

function transferArrayBuffer(buffer) {
  if (isArrayBufferDetached(buffer)) {
    throw new TypeError('Can not transfer a detached array buffer.');
  }
  const target = buffer.slice(0);

  detachArrayBuffer(buffer);

  return target;
}

// https://infra.spec.whatwg.org/#javascript-string-convert
const surrogateRegexp = /(?:[^\uD800-\uDBFF]|^)[\uDC00-\uDFFF]|[\uD800-\uDBFF](?![\uDC00-\uDFFF])/;
function toUSVString(val) {
  const str = `${val}`;
  const match = surrogateRegexp.exec(str);
  if (!match) {
    return str;
  }
  return _toUSVString(str, match.index);
}

function isIterable(o) {
  // checks for null and undefined
  if (o == null) {
    return false;
  }

  return typeof (o)[Symbol.iterator] === 'function';
}

function defineIDLClassMembers(constructor, classStr, members, options) {
  // https://webidl.spec.whatwg.org/#dfn-class-string
  Object.defineProperty(constructor.prototype, Symbol.toStringTag, {
    writable: false,
    enumerable: false,
    configurable: true,
    value: classStr,
  });

  // https://webidl.spec.whatwg.org/#es-operations
  for (const key of members) {
    Object.defineProperty(constructor.prototype, key, {
      enumerable: typeof key !== 'symbol',
    });
  }

  if (options?.iterable) {
    Object.defineProperty(constructor.prototype, Symbol.iterator, {
      writable: true,
      enumerable: false,
      configurable: true,
      value: constructor.prototype.entries,
    });
  }
}

function requireIdlInternalSlot(obj, slot) {
  if (!(slot in obj)) {
    throw TypeError(`${String(slot)} not exists on object`);
  }
}

wrapper.mod = {
  ...native,
  AsyncIteratorPrototype,
  ReflectApply,
  isArrayBufferView,
  transferArrayBuffer,
  toUSVString,
  isIterable,
  defineIDLClassMembers,
  requireIdlInternalSlot,
};
