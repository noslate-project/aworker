'use strict';

// !!!! CODE MODIFIED FROM NODE.JS 10.0.0

const {
  isAnyArrayBuffer,
  isArgumentsObject,
  isDataView,
  isExternal,
  isMap,
  isMapIterator,
  isPromise,
  isSet,
  isSetIterator,
  isWeakMap,
  isWeakSet,
  isRegExp,
  isDate,
  isTypedArray,
  getPromiseDetails,
  kPending,
  kRejected,
} = load('types');

// The build-in Array#join is slower in v8 6.0
function join(output, separator) {
  let str = '';
  if (output.length !== 0) {
    let i = 0;
    for (; i < output.length - 1; i++) {
      // It is faster not to use a template string here
      str += output[i];
      str += separator;
    }
    str += output[i];
  }
  return str;
}

const errorToString = Error.prototype.toString;
const regExpToString = RegExp.prototype.toString;
const dateToISOString = Date.prototype.toISOString;

function objectToString(o) {
  return Object.prototype.toString.call(o);
}

function isError(e) {
  return objectToString(e) === '[object Error]' || e instanceof Error;
}

const colorRegExp = /\u001b\[\d\d?m/g; // eslint-disable-line no-control-regex

function removeColors(str) {
  return str.replace(colorRegExp, '');
}

// Escaped special characters. Use empty strings to fill up unused entries.
const meta = [
  '\\u0000', '\\u0001', '\\u0002', '\\u0003', '\\u0004',
  '\\u0005', '\\u0006', '\\u0007', '\\b', '\\t',
  '\\n', '\\u000b', '\\f', '\\r', '\\u000e',
  '\\u000f', '\\u0010', '\\u0011', '\\u0012', '\\u0013',
  '\\u0014', '\\u0015', '\\u0016', '\\u0017', '\\u0018',
  '\\u0019', '\\u001a', '\\u001b', '\\u001c', '\\u001d',
  '\\u001e', '\\u001f', '', '', '',
  '', '', '', '', "\\'", '', '', '', '', '',
  '', '', '', '', '', '', '', '', '', '',
  '', '', '', '', '', '', '', '', '', '',
  '', '', '', '', '', '', '', '', '', '',
  '', '', '', '', '', '', '', '', '', '',
  '', '', '', '', '', '', '', '\\\\',
];

/* eslint-disable */
const strEscapeSequencesRegExp = /[\x00-\x1f\x27\x5c]/;
const strEscapeSequencesReplacer = /[\x00-\x1f\x27\x5c]/g;
/* eslint-enable */
const keyStrRegExp = /^[a-zA-Z_][a-zA-Z_0-9]*$/;
const numberRegExp = /^(0|[1-9][0-9]*)$/;

const escapeFn = str => meta[str.charCodeAt(0)];

const MIN_LINE_LENGTH = 16;

const readableRegExps = {};
let CIRCULAR_ERROR_MESSAGE;

const inspectDefaultOptions = Object.seal({
  showHidden: false,
  depth: 2,
  colors: false,
  customInspect: true,
  showProxy: false,
  maxArrayLength: 100,
  breakLength: 60,
  compact: true,
});

// Escape control characters, single quotes and the backslash.
// This is similar to JSON stringify escaping.
function strEscape(str) {
  // Some magic numbers that worked out fine while benchmarking with v8 6.0
  if (str.length < 5000 && !strEscapeSequencesRegExp.test(str)) {
    return `'${str}'`;
  }

  if (str.length > 100) {
    return `'${str.replace(strEscapeSequencesReplacer, escapeFn)}'`;
  }
  let result = '';
  let last = 0;
  let i = 0;
  for (; i < str.length; i++) {
    const point = str.charCodeAt(i);
    if (point === 39 || point === 92 || point < 32) {
      if (last === i) {
        result += meta[point];
      } else {
        result += `${str.slice(last, i)}${meta[point]}`;
      }
      last = i + 1;
    }
  }
  if (last === 0) {
    result = str;
  } else if (last !== i) {
    result += str.slice(last);
  }
  return `'${result}'`;
}

function stylizeNoColor(str/* , styleType */) {
  return str;
}

function stylizeWithColor(str, styleType) {
  const style = inspect.styles[styleType];
  if (style !== undefined) {
    const color = inspect.colors[style];
    return `\u001b[${color[0]}m${str}\u001b[${color[1]}m`;
  }
  return str;
}

function formatNumber(fn, value) {
  // Format -0 as '-0'. Checking `value === -0` won't distinguish 0 from -0.
  if (Object.is(value, -0)) {
    return fn('-0', 'number');
  }
  return fn(`${value}`, 'number');
}

function formatPrimitive(fn, value, ctx) {
  if (typeof value === 'string') {
    if (ctx.compact === false &&
      ctx.indentationLvl + value.length > ctx.breakLength &&
      value.length > MIN_LINE_LENGTH) {
      const minLineLength = Math.max(ctx.breakLength - ctx.indentationLvl, MIN_LINE_LENGTH);
      const averageLineLength = Math.ceil(value.length / Math.ceil(value.length / minLineLength));
      const divisor = Math.max(averageLineLength, MIN_LINE_LENGTH);
      let res = '';
      if (readableRegExps[divisor] === undefined) {
        readableRegExps[divisor] = new RegExp(`(.|\\n){1,${divisor}}(\\s|$)|(\\n|.)+?(\\s|$)`, 'gm');
      }
      const matches = value.match(readableRegExps[divisor]);
      if (matches.length > 1) {
        const indent = ' '.repeat(ctx.indentationLvl);
        res += `${fn(strEscape(matches[0]), 'string')} +\n`;
        let i = 1;
        for (; i < matches.length - 1; i++) {
          res += `${indent}  ${fn(strEscape(matches[i]), 'string')} +\n`;
        }
        res += `${indent}  ${fn(strEscape(matches[i]), 'string')}`;
        return res;
      }
    }
    return fn(strEscape(value), 'string');
  }
  if (typeof value === 'number') {
    return formatNumber(fn, value);
  }
  if (typeof value === 'bigint') {
    return fn(`${value}n`, 'bigint');
  }
  if (typeof value === 'boolean') {
    return fn(`${value}`, 'boolean');
  }
  if (typeof value === 'undefined') {
    return fn('undefined', 'undefined');
  }
  // es6 symbol primitive
  return fn(value.toString(), 'symbol');
}

function formatError(value) {
  return value.stack || `[${errorToString.call(value)}]`;
}

function formatProperty(ctx, value, recurseTimes, key, array) {
  let name;
  let str;
  const desc = Object.getOwnPropertyDescriptor(value, key) ||
    { value: value[key], enumerable: true };
  if (desc.value !== undefined) {
    const diff = array !== 0 || ctx.compact === false ? 2 : 3;
    ctx.indentationLvl += diff;
    str = formatValue(ctx, desc.value, recurseTimes, array === 0);
    ctx.indentationLvl -= diff;
  } else if (desc.get !== undefined) {
    if (desc.set !== undefined) {
      str = ctx.stylize('[Getter/Setter]', 'special');
    } else {
      str = ctx.stylize('[Getter]', 'special');
    }
  } else if (desc.set !== undefined) {
    str = ctx.stylize('[Setter]', 'special');
  } else {
    str = ctx.stylize('undefined', 'undefined');
  }
  if (array === 1) {
    return str;
  }
  if (typeof key === 'symbol') {
    name = `[${ctx.stylize(key.toString(), 'symbol')}]`;
  } else if (desc.enumerable === false) {
    name = `[${key}]`;
  } else if (keyStrRegExp.test(key)) {
    name = ctx.stylize(key, 'name');
  } else {
    name = ctx.stylize(strEscape(key), 'string');
  }

  return `${name}: ${str}`;
}

function formatObject(ctx, value, recurseTimes, keys) {
  const len = keys.length;
  const output = new Array(len);
  for (let i = 0; i < len; i++) {
    output[i] = formatProperty(ctx, value, recurseTimes, keys[i], 0);
  }
  return output;
}

function getConstructorName(obj) {
  while (obj) {
    const descriptor = Object.getOwnPropertyDescriptor(obj, 'constructor');
    if (descriptor !== undefined &&
        typeof descriptor.value === 'function' &&
        descriptor.value.name !== '') {
      return descriptor.value.name;
    }

    obj = Object.getPrototypeOf(obj);
  }

  return '';
}

function getPrefix(constructor, tag) {
  if (constructor !== '') {
    if (tag !== '' && constructor !== tag) {
      return `${constructor} [${tag}] `;
    }
    return `${constructor} `;
  }

  if (tag !== '') {
    return `[${tag}] `;
  }

  return '';
}

function formatSpecialArray(ctx, value, recurseTimes, keys, maxLength, valLen) {
  const output = [];
  const keyLen = keys.length;
  let visibleLength = 0;
  let i = 0;
  if (keyLen !== 0 && numberRegExp.test(keys[0])) {
    for (const key of keys) {
      if (visibleLength === maxLength) {
        break;
      }
      const index = +key;
      // Arrays can only have up to 2^32 - 1 entries
      if (index > 2 ** 32 - 2) {
        break;
      }
      if (i !== index) {
        if (!numberRegExp.test(key)) {
          break;
        }
        const emptyItems = index - i;
        const ending = emptyItems > 1 ? 's' : '';
        const message = `<${emptyItems} empty item${ending}>`;
        output.push(ctx.stylize(message, 'undefined'));
        i = index;
        if (++visibleLength === maxLength) {
          break;
        }
      }
      output.push(formatProperty(ctx, value, recurseTimes, key, 1));
      visibleLength++;
      i++;
    }
  }
  if (i < valLen && visibleLength !== maxLength) {
    const len = valLen - i;
    const ending = len > 1 ? 's' : '';
    const message = `<${len} empty item${ending}>`;
    output.push(ctx.stylize(message, 'undefined'));
    i = valLen;
    if (keyLen === 0) {
      return output;
    }
  }
  const remaining = valLen - i;
  if (remaining > 0) {
    output.push(`... ${remaining} more item${remaining > 1 ? 's' : ''}`);
  }
  if (ctx.showHidden && keys[keyLen - 1] === 'length') {
    // No extra keys
    output.push(formatProperty(ctx, value, recurseTimes, 'length', 2));
  } else if (valLen === 0 ||
    keyLen > valLen && keys[valLen - 1] === `${valLen - 1}`) {
    // The array is not sparse
    for (i = valLen; i < keyLen; i++) {
      output.push(formatProperty(ctx, value, recurseTimes, keys[i], 2));
    }
  } else if (keys[keyLen - 1] !== `${valLen - 1}`) {
    const extra = [];
    // Only handle special keys
    let key;
    for (i = keys.length - 1; i >= 0; i--) {
      key = keys[i];
      if (numberRegExp.test(key) && +key < 2 ** 32 - 1) {
        break;
      }
      extra.push(formatProperty(ctx, value, recurseTimes, key, 2));
    }
    for (i = extra.length - 1; i >= 0; i--) {
      output.push(extra[i]);
    }
  }
  return output;
}

function formatArray(ctx, value, recurseTimes, keys) {
  const len = Math.min(Math.max(0, ctx.maxArrayLength), value.length);
  const hidden = ctx.showHidden ? 1 : 0;
  const valLen = value.length;
  const keyLen = keys.length - hidden;
  if (keyLen !== valLen || keys[keyLen - 1] !== `${valLen - 1}`) {
    return formatSpecialArray(ctx, value, recurseTimes, keys, len, valLen);
  }

  const remaining = valLen - len;
  const output = new Array(len + (remaining > 0 ? 1 : 0) + hidden);
  let i = 0;
  for (; i < len; i++) {
    output[i] = formatProperty(ctx, value, recurseTimes, keys[i], 1);
  }
  if (remaining > 0) {
    output[i++] = `... ${remaining} more item${remaining > 1 ? 's' : ''}`;
  }
  if (ctx.showHidden === true) {
    output[i] = formatProperty(ctx, value, recurseTimes, 'length', 2);
  }
  return output;
}

function formatTypedArray(ctx, value, recurseTimes, keys) {
  const maxLength = Math.min(Math.max(0, ctx.maxArrayLength), value.length);
  const remaining = value.length - maxLength;
  const output = new Array(maxLength + (remaining > 0 ? 1 : 0));
  let i = 0;
  for (; i < maxLength; ++i) {
    output[i] = formatNumber(ctx.stylize, value[i]);
  }
  if (remaining > 0) {
    output[i] = `... ${remaining} more item${remaining > 1 ? 's' : ''}`;
  }
  if (ctx.showHidden) {
    // .buffer goes last, it's not a primitive like the others.
    const extraKeys = [
      'BYTES_PER_ELEMENT',
      'length',
      'byteLength',
      'byteOffset',
      'buffer',
    ];
    for (i = 0; i < extraKeys.length; i++) {
      const str = formatValue(ctx, value[extraKeys[i]], recurseTimes);
      output.push(`[${extraKeys[i]}]: ${str}`);
    }
  }
  // TypedArrays cannot have holes. Therefore it is safe to assume that all
  // extra keys are indexed after value.length.
  for (i = value.length; i < keys.length; i++) {
    output.push(formatProperty(ctx, value, recurseTimes, keys[i], 2));
  }
  return output;
}

function formatSet(ctx, value, recurseTimes, keys) {
  const output = new Array(value.size + keys.length + (ctx.showHidden ? 1 : 0));
  let i = 0;
  for (const v of value) {
    output[i++] = formatValue(ctx, v, recurseTimes);
  }
  // With `showHidden`, `length` will display as a hidden property for
  // arrays. For consistency's sake, do the same for `size`, even though this
  // property isn't selected by Object.getOwnPropertyNames().
  if (ctx.showHidden) {
    output[i++] = `[size]: ${ctx.stylize(`${value.size}`, 'number')}`;
  }
  for (let n = 0; n < keys.length; n++) {
    output[i++] = formatProperty(ctx, value, recurseTimes, keys[n], 0);
  }
  return output;
}

function formatMap(ctx, value, recurseTimes, keys) {
  const output = new Array(value.size + keys.length + (ctx.showHidden ? 1 : 0));
  let i = 0;
  for (const [ k, v ] of value) {
    output[i++] = `${formatValue(ctx, k, recurseTimes)} => ` +
      formatValue(ctx, v, recurseTimes);
  }
  // See comment in formatSet
  if (ctx.showHidden) {
    output[i++] = `[size]: ${ctx.stylize(`${value.size}`, 'number')}`;
  }
  for (let n = 0; n < keys.length; n++) {
    output[i++] = formatProperty(ctx, value, recurseTimes, keys[n], 0);
  }
  return output;
}

function formatWeakSet(/** ctx, value, recurseTimes, keys */) {
  // TODO(kaidi.zkd): preview
  return '[WeakSet]';
  // const maxArrayLength = Math.max(ctx.maxArrayLength, 0);
  // const entries = previewWeakSet(value, maxArrayLength + 1);
  // const maxLength = Math.min(maxArrayLength, entries.length);
  // let output = new Array(maxLength);
  // for (var i = 0; i < maxLength; ++i)
  //   output[i] = formatValue(ctx, entries[i], recurseTimes);
  // // Sort all entries to have a halfway reliable output (if more entries than
  // // retrieved ones exist, we can not reliably return the same output).
  // output = output.sort();
  // if (entries.length > maxArrayLength)
  //   output.push('... more items');
  // for (i = 0; i < keys.length; i++)
  //   output.push(formatProperty(ctx, value, recurseTimes, keys[i], 0));
  // return output;
}

function formatWeakMap(/** ctx, value, recurseTimes, keys */) {
  // TODO(kaidi.zkd): preview
  return '[WeakMap]';
  // const maxArrayLength = Math.max(ctx.maxArrayLength, 0);
  // const entries = previewWeakMap(value, maxArrayLength + 1);
  // // Entries exist as [key1, val1, key2, val2, ...]
  // const remainder = entries.length / 2 > maxArrayLength;
  // const len = entries.length / 2 - (remainder ? 1 : 0);
  // const maxLength = Math.min(maxArrayLength, len);
  // let output = new Array(maxLength);
  // for (var i = 0; i < len; i++) {
  //   const pos = i * 2;
  //   output[i] = `${formatValue(ctx, entries[pos], recurseTimes)} => ` +
  //     formatValue(ctx, entries[pos + 1], recurseTimes);
  // }
  // // Sort all entries to have a halfway reliable output (if more entries than
  // // retrieved ones exist, we can not reliably return the same output).
  // output = output.sort();
  // if (remainder > 0)
  //   output.push('... more items');
  // for (i = 0; i < keys.length; i++)
  //   output.push(formatProperty(ctx, value, recurseTimes, keys[i], 0));
  // return output;
}

// function formatCollectionIterator(preview, ctx, value, recurseTimes, keys) {
//   const output = [];
//   for (const entry of preview(value)) {
//     if (ctx.maxArrayLength === output.length) {
//       output.push('... more items');
//       break;
//     }
//     output.push(formatValue(ctx, entry, recurseTimes));
//   }
//   for (let n = 0; n < keys.length; n++) {
//     output.push(formatProperty(ctx, value, recurseTimes, keys[n], 0));
//   }
//   return output;
// }

function formatMapIterator(/** ctx, value, recurseTimes, keys */) {
  // TODO(kaidi.zkd): preview
  return '[MapIterator]';
  // return formatCollectionIterator(previewMapIterator, ctx, value, recurseTimes,
  //                                 keys);
}

function formatSetIterator(/* ctx, value, recurseTimes, keys */) {
  // TODO(kaidi.zkd): preview
  return '[SetIterator]';
  // return formatCollectionIterator(previewSetIterator, ctx, value, recurseTimes,
  //                                 keys);
}

function formatPromise(ctx, value, recurseTimes, keys) {
  let output;
  const [ state, result ] = getPromiseDetails(value);
  if (state === kPending) {
    output = [ '<pending>' ];
  } else {
    const str = formatValue(ctx, result, recurseTimes);
    output = [ state === kRejected ? `<rejected> ${str}` : str ];
  }
  for (let n = 0; n < keys.length; n++) {
    output.push(formatProperty(ctx, value, recurseTimes, keys[n], 0));
  }
  return output;
}

function reduceToSingleString(ctx, output, base, braces, addLn) {
  const breakLength = ctx.breakLength;
  let i = 0;
  if (ctx.compact === false) {
    const indentation = ' '.repeat(ctx.indentationLvl);
    let res = `${base ? `${base} ` : ''}${braces[0]}\n${indentation}  `;
    for (; i < output.length - 1; i++) {
      res += `${output[i]},\n${indentation}  `;
    }
    res += `${output[i]}\n${indentation}${braces[1]}`;
    return res;
  }
  if (output.length * 2 <= breakLength) {
    let length = 0;
    for (; i < output.length && length <= breakLength; i++) {
      if (ctx.colors) {
        length += removeColors(output[i]).length + 1;
      } else {
        length += output[i].length + 1;
      }
    }
    if (length <= breakLength) {
      return `${braces[0]}${base ? ` ${base}` : ''} ${join(output, ', ')} ` +
        braces[1];
    }
  }
  // If the opening "brace" is too large, like in the case of "Set {",
  // we need to force the first item to be on the next line or the
  // items will not line up correctly.
  const indentation = ' '.repeat(ctx.indentationLvl);
  const extraLn = addLn === true ? `\n${indentation}` : '';
  const ln = base === '' && braces[0].length === 1 ?
    ' ' : `${base ? ` ${base}` : base}\n${indentation}  `;
  const str = join(output, `,\n${indentation}  `);
  return `${extraLn}${braces[0]}${ln}${str} ${braces[1]}`;
}

function formatValue(ctx, value, recurseTimes, ln) {
  // Primitive types cannot have properties
  if (typeof value !== 'object' && typeof value !== 'function') {
    return formatPrimitive(ctx.stylize, value, ctx);
  }
  if (value === null) {
    return ctx.stylize('null', 'null');
  }

  // TODO(kaidi.zkd): inspect proxy
  // if (ctx.showProxy) {
  //   const proxy = getProxyDetails(value);
  //   if (proxy !== undefined) {
  //     if (recurseTimes != null) {
  //       if (recurseTimes < 0)
  //         return ctx.stylize('Proxy [Array]', 'special');
  //       recurseTimes -= 1;
  //     }
  //     ctx.indentationLvl += 2;
  //     const res = [
  //       formatValue(ctx, proxy[0], recurseTimes),
  //       formatValue(ctx, proxy[1], recurseTimes)
  //     ];
  //     ctx.indentationLvl -= 2;
  //     const str = reduceToSingleString(ctx, res, '', ['[', ']']);
  //     return `Proxy ${str}`;
  //   }
  // }

  // Provide a hook for user-specified inspect functions.
  // Check that value is an object with an inspect function on it
  if (ctx.customInspect) {
    const maybeCustom = value[mod.customInspectSymbol];

    if (typeof maybeCustom === 'function' &&
        // Filter out the util module, its inspect function is special
        maybeCustom !== inspect &&
        // Also filter out any prototype objects using the circular check.
        !(value.constructor && value.constructor.prototype === value)) {
      const ret = maybeCustom.call(value, recurseTimes, ctx);

      // If the custom inspection method returned `this`, don't go into
      // infinite recursion.
      if (ret !== value) {
        if (typeof ret !== 'string') {
          return formatValue(ctx, ret, recurseTimes);
        }
        return ret;
      }
    }
  }

  // Using an array here is actually better for the average case than using
  // a Set. `seen` will only check for the depth and will never grow too large.
  if (ctx.seen.indexOf(value) !== -1) {
    return ctx.stylize('[Circular]', 'special');
  }

  let keys;
  let symbols = Object.getOwnPropertySymbols(value);

  // Look up the keys of the object.
  if (ctx.showHidden) {
    keys = Object.getOwnPropertyNames(value);
  } else {
    keys = Object.keys(value);
    if (symbols.length !== 0) {
      symbols = symbols.filter(key => propertyIsEnumerable.call(value, key));
    }
  }

  const keyLength = keys.length + symbols.length;

  const constructor = getConstructorName(value);
  let tag = value[Symbol.toStringTag];
  if (typeof tag !== 'string') {
    tag = '';
  }
  let base = '';
  let formatter = formatObject;
  let braces;
  let noIterator = true;
  let raw;
  let extra;

  // Iterators and the rest are split to reduce checks
  if (value[Symbol.iterator]) {
    noIterator = false;
    if (Array.isArray(value)) {
      // Only set the constructor for non ordinary ("Array [...]") arrays.
      const prefix = getPrefix(constructor, tag);
      braces = [ `${prefix === 'Array ' ? '' : prefix}[`, ']' ];
      if (value.length === 0 && keyLength === 0) {
        return `${braces[0]}]`;
      }
      formatter = formatArray;
    } else if (isSet(value)) {
      const prefix = getPrefix(constructor, tag);
      if (value.size === 0 && keyLength === 0) {
        return `${prefix}{}`;
      }
      braces = [ `${prefix}{`, '}' ];
      formatter = formatSet;
    } else if (isMap(value)) {
      const prefix = getPrefix(constructor, tag);
      if (value.size === 0 && keyLength === 0) {
        return `${prefix}{}`;
      }
      braces = [ `${prefix}{`, '}' ];
      formatter = formatMap;
    } else if (isTypedArray(value)) {
      braces = [ `${getPrefix(constructor, tag)}[`, ']' ];
      formatter = formatTypedArray;
    } else if (isMapIterator(value)) {
      braces = [ `[${tag}] {`, '}' ];
      formatter = formatMapIterator;
    } else if (isSetIterator(value)) {
      braces = [ `[${tag}] {`, '}' ];
      formatter = formatSetIterator;
    } else {
      // Check for boxed strings with valueOf()
      // The .valueOf() call can fail for a multitude of reasons
      try {
        raw = value.valueOf();
      } catch (e) { /* ignore */ }

      if (typeof raw === 'string') {
        const formatted = formatPrimitive(stylizeNoColor, raw, ctx);
        if (keyLength === raw.length) {
          return ctx.stylize(`[String: ${formatted}]`, 'string');
        }
        base = `[String: ${formatted}]`;
        // For boxed Strings, we have to remove the 0-n indexed entries,
        // since they just noisy up the output and are redundant
        // Make boxed primitive Strings look like such
        keys = keys.slice(value.length);
        braces = [ '{', '}' ];
      } else {
        noIterator = true;
      }
    }
  }
  if (noIterator) {
    braces = [ '{', '}' ];
    if (constructor === 'Object') {
      if (isArgumentsObject(value)) {
        braces[0] = '[Arguments] {';
        if (keyLength === 0) {
          return '[Arguments] {}';
        }
      } else if (tag !== '') {
        braces[0] = `${getPrefix(constructor, tag)}{`;
        if (keyLength === 0) {
          return `${braces[0]}}`;
        }
      } else if (keyLength === 0) {
        return '{}';
      }
    } else if (typeof value === 'function') {
      const name =
        `${constructor || tag}${value.name ? `: ${value.name}` : ''}`;
      if (keyLength === 0) {
        return ctx.stylize(`[${name}]`, 'special');
      }
      base = `[${name}]`;
    } else if (isRegExp(value)) {
      // Make RegExps say that they are RegExps
      if (keyLength === 0 || recurseTimes < 0) {
        return ctx.stylize(regExpToString.call(value), 'regexp');
      }
      base = `${regExpToString.call(value)}`;
    } else if (isDate(value)) {
      if (keyLength === 0) {
        if (Number.isNaN(value.getTime())) {
          return ctx.stylize(value.toString(), 'date');
        }
        return ctx.stylize(dateToISOString.call(value), 'date');
      }
      // Make dates with properties first say the date
      base = `${dateToISOString.call(value)}`;
    } else if (isError(value)) {
      // Make error with message first say the error
      if (keyLength === 0) {
        return formatError(value);
      }
      base = `${formatError(value)}`;
    } else if (isAnyArrayBuffer(value)) {
      // Fast path for ArrayBuffer and SharedArrayBuffer.
      // Can't do the same for DataView because it has a non-primitive
      // .buffer property that we need to recurse for.
      const prefix = getPrefix(constructor, tag);
      if (keyLength === 0) {
        return prefix +
              `{ byteLength: ${formatNumber(ctx.stylize, value.byteLength)} }`;
      }
      braces[0] = `${prefix}{`;
      keys.unshift('byteLength');
    } else if (isDataView(value)) {
      braces[0] = `${getPrefix(constructor, tag)}{`;
      // .buffer goes last, it's not a primitive like the others.
      keys.unshift('byteLength', 'byteOffset', 'buffer');
    } else if (isPromise(value)) {
      braces[0] = `${getPrefix(constructor, tag)}{`;
      formatter = formatPromise;
    } else if (isWeakSet(value)) {
      braces[0] = `${getPrefix(constructor, tag)}{`;
      if (ctx.showHidden) {
        formatter = formatWeakSet;
      } else {
        extra = '[items unknown]';
      }
    } else if (isWeakMap(value)) {
      braces[0] = `${getPrefix(constructor, tag)}{`;
      if (ctx.showHidden) {
        formatter = formatWeakMap;
      } else {
        extra = '[items unknown]';
      }
    } else {
      // Check boxed primitives other than string with valueOf()
      // NOTE: `Date` has to be checked first!
      // The .valueOf() call can fail for a multitude of reasons
      try {
        raw = value.valueOf();
      } catch (e) { /* ignore */ }

      if (typeof raw === 'number') {
        // Make boxed primitive Numbers look like such
        const formatted = formatPrimitive(stylizeNoColor, raw);
        if (keyLength === 0) {
          return ctx.stylize(`[Number: ${formatted}]`, 'number');
        }
        base = `[Number: ${formatted}]`;
      } else if (typeof raw === 'boolean') {
        // Make boxed primitive Booleans look like such
        const formatted = formatPrimitive(stylizeNoColor, raw);
        if (keyLength === 0) {
          return ctx.stylize(`[Boolean: ${formatted}]`, 'boolean');
        }
        base = `[Boolean: ${formatted}]`;
      } else if (typeof raw === 'bigint') {
        // Make boxed primitive BigInts look like such
        const formatted = formatPrimitive(stylizeNoColor, raw);
        if (keyLength === 0) {
          return ctx.stylize(`[BigInt: ${formatted}]`, 'bigint');
        }
        base = `[BigInt: ${formatted}]`;
      } else if (typeof raw === 'symbol') {
        const formatted = formatPrimitive(stylizeNoColor, raw);
        return ctx.stylize(`[Symbol: ${formatted}]`, 'symbol');
      } else if (keyLength === 0) {
        if (isExternal(value)) {
          return ctx.stylize('[External]', 'special');
        }
        return `${getPrefix(constructor, tag)}{}`;
      } else {
        braces[0] = `${getPrefix(constructor, tag)}{`;
      }
    }
  }

  if (recurseTimes != null) {
    if (recurseTimes < 0) {
      return ctx.stylize(`[${constructor || tag || 'Object'}]`, 'special');
    }
    recurseTimes -= 1;
  }

  ctx.seen.push(value);
  const output = formatter(ctx, value, recurseTimes, keys);

  if (extra !== undefined) {
    output.unshift(extra);
  }

  for (let i = 0; i < symbols.length; i++) {
    output.push(formatProperty(ctx, value, recurseTimes, symbols[i], 0));
  }
  ctx.seen.pop();

  return reduceToSingleString(ctx, output, base, braces, ln);
}

function inspect(value, opts) {
  // Default options
  const ctx = {
    seen: [],
    stylize: stylizeNoColor,
    showHidden: inspectDefaultOptions.showHidden,
    depth: inspectDefaultOptions.depth,
    colors: inspectDefaultOptions.colors,
    customInspect: inspectDefaultOptions.customInspect,
    showProxy: inspectDefaultOptions.showProxy,
    // TODO(BridgeAR): Deprecate `maxArrayLength` and replace it with
    // `maxEntries`.
    maxArrayLength: inspectDefaultOptions.maxArrayLength,
    breakLength: inspectDefaultOptions.breakLength,
    indentationLvl: 0,
    compact: inspectDefaultOptions.compact,
  };
  // Legacy...
  if (arguments.length > 2) {
    if (arguments[2] !== undefined) {
      ctx.depth = arguments[2];
    }
    if (arguments.length > 3 && arguments[3] !== undefined) {
      ctx.colors = arguments[3];
    }
  }
  // Set user-specified options
  if (typeof opts === 'boolean') {
    ctx.showHidden = opts;
  } else if (opts) {
    const optKeys = Object.keys(opts);
    for (let i = 0; i < optKeys.length; i++) {
      ctx[optKeys[i]] = opts[optKeys[i]];
    }
  }
  if (ctx.colors) ctx.stylize = stylizeWithColor;
  if (ctx.maxArrayLength === null) ctx.maxArrayLength = Infinity;
  return formatValue(ctx, value, ctx.depth);
}

// http://en.wikipedia.org/wiki/ANSI_escape_code#graphics
inspect.colors = Object.assign(Object.create(null), {
  bold: [ 1, 22 ],
  italic: [ 3, 23 ],
  underline: [ 4, 24 ],
  inverse: [ 7, 27 ],
  white: [ 37, 39 ],
  grey: [ 90, 39 ],
  black: [ 30, 39 ],
  blue: [ 34, 39 ],
  cyan: [ 36, 39 ],
  green: [ 32, 39 ],
  magenta: [ 35, 39 ],
  red: [ 31, 39 ],
  yellow: [ 33, 39 ],
});

// Don't use 'blue' not visible on cmd.exe
inspect.styles = Object.assign(Object.create(null), {
  special: 'cyan',
  number: 'yellow',
  bigint: 'yellow',
  boolean: 'yellow',
  undefined: 'grey',
  null: 'bold',
  string: 'green',
  symbol: 'green',
  date: 'magenta',
  // "name": intentionally not styling
  regexp: 'red',
});

function tryStringify(arg) {
  try {
    return JSON.stringify(arg);
  } catch (err) {
    // Populate the circular error message lazily
    if (!CIRCULAR_ERROR_MESSAGE) {
      try {
        const a = {}; a.a = a; JSON.stringify(a);
      } catch (err) {
        CIRCULAR_ERROR_MESSAGE = err.message;
      }
    }
    if (err.name === 'TypeError' && err.message === CIRCULAR_ERROR_MESSAGE) {
      return '[Circular]';
    }
    throw err;
  }
}

mod.formatWithOptions = function formatWithOptions(inspectOptions, f) {
  let i;
  let tempStr;
  if (typeof f !== 'string') {
    if (arguments.length === 1) return '';
    let res = '';
    for (i = 1; i < arguments.length - 1; i++) {
      res += inspect(arguments[i], inspectOptions);
      res += ' ';
    }
    res += inspect(arguments[i], inspectOptions);
    return res;
  }

  if (arguments.length === 2) return f;

  let str = '';
  let a = 2;
  let lastPos = 0;
  for (i = 0; i < f.length - 1; i++) {
    if (f.charCodeAt(i) === 37) { // '%'
      const nextChar = f.charCodeAt(++i);
      if (a !== arguments.length) {
        switch (nextChar) {
          case 115: // 's'
            tempStr = String(arguments[a++]);
            break;
          case 106: // 'j'
            tempStr = tryStringify(arguments[a++]);
            break;
          case 100: // 'd'
            tempStr = `${Number(arguments[a++])}`;
            break;
          case 79: // 'O'
            tempStr = inspect(arguments[a++], inspectOptions);
            break;
          case 111: // 'o'
          {
            const opts = Object.assign({}, inspectOptions, {
              showHidden: true,
              showProxy: true,
              depth: 4,
            });
            tempStr = inspect(arguments[a++], opts);
            break;
          }
          case 105: // 'i'
            tempStr = `${parseInt(arguments[a++])}`;
            break;
          case 102: // 'f'
            tempStr = `${parseFloat(arguments[a++])}`;
            break;
          case 37: // '%'
            str += f.slice(lastPos, i);
            lastPos = i + 1;
            continue;
          default: // any other character is not a correct placeholder
            continue;
        }
        if (lastPos !== i - 1) {
          str += f.slice(lastPos, i - 1);
        }
        str += tempStr;
        lastPos = i + 1;
      } else if (nextChar === 37) {
        str += f.slice(lastPos, i);
        lastPos = i + 1;
      }
    }
  }
  if (lastPos === 0) {
    str = f;
  } else if (lastPos < f.length) {
    str += f.slice(lastPos);
  }
  while (a < arguments.length) {
    const x = arguments[a++];
    if ((typeof x !== 'object' && typeof x !== 'symbol') || x === null) {
      str += ` ${x}`;
    } else {
      str += ` ${inspect(x, inspectOptions)}`;
    }
  }
  return str;
};

mod.customInspectSymbol = Symbol('customInspect');
