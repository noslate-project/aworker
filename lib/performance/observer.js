'use strict';

const {
  isPerformanceEntry,
} = load('performance/performance_entry');
const { trustedDispatchEvent } = load('dom/event_target');
const { Event } = load('dom/event');
const { DOMException } = load('dom/exception');
const { formatWithOptions, customInspectSymbol: kInspect } = load('console/format');

const {
  setTimeout,
  setImmediate,
} = load('timer');

const kBuffer = Symbol('kBuffer');
const kCallback = Symbol('kCallback');
const kDispatch = Symbol('kDispatch');
const kEntryTypes = Symbol('kEntryTypes');
const kMaybeBuffer = Symbol('kMaybeBuffer');
const kType = Symbol('kType');

const kTypeSingle = 0;
const kTypeMultiple = 1;
const resourceTimingBufferSizeLimit = 250;
let resourceTimingBufferFullPending = false;

let resourceTimingSecondaryBuffer = [];

const kSupportedEntryTypes = Object.freeze([
  'mark',
  'measure',
  'resource',
]);
const kUserTimingEntryTypes = Object.freeze([
  'mark',
  'measure',
]);

const kPerformanceEntryBuffer = new Map();
const kObservers = new Set();
let pendingObservers = new Set();
let isPending = false;

function queuePending() {
  if (isPending) return;
  isPending = true;
  setTimeout(() => {
    isPending = false;
    const po = pendingObservers;
    pendingObservers = new Set();
    for (const observer of po) { observer[kDispatch](); }
  }, 0);
}

class PerformanceObserverEntryList {
  constructor(entries) {
    this[kBuffer] = entries.sort((first, second) => {
      if (first.startTime < second.startTime) return -1;
      if (first.startTime > second.startTime) return 1;
      return 0;
    });
  }

  getEntries() {
    return this[kBuffer].slice();
  }

  getEntriesByType(type) {
    type = `${type}`;
    return this[kBuffer].filter(
      entry => entry.entryType === type);
  }

  getEntriesByName(name, type) {
    name = `${name}`;
    type = type != null ? `${type}` : undefined;
    return this[kBuffer].filter(
      entry => entry.name === name && (type === undefined || entry.entryType === type));
  }

  [kInspect](depth, options) {
    if (depth < 0) return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `PerformanceObserverEntryList ${formatWithOptions(opts, this[kBuffer])}`;
  }
}

class PerformanceObserver {
  constructor(callback) {
    // TODO(joyeecheung): V8 snapshot does not support instance member
    // initializers for now:
    // https://bugs.chromium.org/p/v8/issues/detail?id=10704
    this[kBuffer] = [];
    this[kEntryTypes] = new Set();
    this[kType] = undefined;
    if (typeof callback !== 'function') {
      throw new TypeError();
    }
    this[kCallback] = callback;
  }

  observe(options = {}) {
    options = options ?? {};
    if (typeof options !== 'object') {
      throw new TypeError();
    }
    const {
      entryTypes,
      type,
      buffered,
    } = { ...options };
    if (entryTypes === undefined && type === undefined) { throw new TypeError('Missing options.entryTypes and options.type'); }
    if (entryTypes !== undefined && (type !== undefined || buffered)) { throw new TypeError(); }

    if ((this[kType] === kTypeMultiple && type) || (this[kType] === kTypeSingle && entryTypes)) {
      throw new DOMException('PerformanceObserver already connected with different types', 'InvalidModificationError');
    }

    switch (this[kType]) {
      case undefined:
        if (entryTypes !== undefined) this[kType] = kTypeMultiple;
        if (type !== undefined) this[kType] = kTypeSingle;
        break;
      case kTypeSingle:
        if (entryTypes !== undefined) { throw new TypeError(`Invalid arg value options.entryTypes ${entryTypes}`); }
        break;
      case kTypeMultiple:
        if (type !== undefined) { throw new TypeError(`Invalid arg value options.type ${type}`); }
        break;
      default:
    }

    if (this[kType] === kTypeMultiple) {
      if (!Array.isArray(entryTypes)) {
        throw new TypeError(
          `Invalid argument type options.entryTypes, expect string[], but got ${entryTypes}`);
      }
      this[kEntryTypes].clear();
      for (let n = 0; n < entryTypes.length; n++) {
        if (kSupportedEntryTypes.includes(entryTypes[n])) {
          this[kEntryTypes].add(entryTypes[n]);
        }
      }
    } else {
      if (!kSupportedEntryTypes.includes(type)) { return; }
      this[kEntryTypes].add(type);
      if (buffered) {
        const entries = filterBufferMapByNameAndType(undefined, type);
        this[kBuffer].push(...entries);
        pendingObservers.add(this);
        if (pendingObservers.size) { queuePending(); }
      }
    }

    if (this[kEntryTypes].size) {
      kObservers.add(this);
    } else {
      this.disconnect();
    }
  }

  disconnect() {
    kObservers.delete(this);
    pendingObservers.delete(this);
    this[kBuffer] = [];
    this[kEntryTypes].clear();
    this[kType] = undefined;
  }

  takeRecords() {
    const list = this[kBuffer];
    this[kBuffer] = [];
    return list;
  }

  static get supportedEntryTypes() {
    return kSupportedEntryTypes;
  }

  [kMaybeBuffer](entry) {
    if (!this[kEntryTypes].has(entry.entryType)) { return; }
    this[kBuffer].push(entry);
    pendingObservers.add(this);
    if (pendingObservers.size) { queuePending(); }
  }

  [kDispatch]() { this[kCallback](new PerformanceObserverEntryList(this.takeRecords()), this); }

  [kInspect](depth, options) {
    if (depth < 0) return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `PerformanceObserver ${formatWithOptions(opts, {
      connected: kObservers.has(this),
      pending: pendingObservers.has(this),
      entryTypes: Array.from(this[kEntryTypes]),
      buffer: this[kBuffer],
    })}`;
  }
}

function enqueue(entry) {
  if (!isPerformanceEntry(entry)) { throw new TypeError(`Invalid entry, expect PerformanceEntry, but got ${entry}`); }
  if (kObservers.size === 0) {
    return;
  }

  for (const obs of kObservers) {
    obs[kMaybeBuffer](entry);
  }
}

function clearEntriesFromBuffer(type, name) {
  if (name === undefined) {
    kPerformanceEntryBuffer.delete(type);
    return;
  }
  let buffer = getEntryBuffer(type);
  buffer = buffer.filter(
    entry => entry.name !== name);
  kPerformanceEntryBuffer.set(type, buffer);
}

function getEntryBuffer(type) {
  let buffer = kPerformanceEntryBuffer.get(type);
  if (buffer === undefined) {
    buffer = [];
    kPerformanceEntryBuffer.set(type, buffer);
  }
  return buffer;
}

function filterBufferMapByNameAndType(name, type) {
  let bufferList;
  if (type !== undefined) {
    bufferList = kPerformanceEntryBuffer.has(type) ? [ kPerformanceEntryBuffer.get(type) ] : [];
  } else {
    bufferList = Array.from(kPerformanceEntryBuffer.values());
  }
  return bufferList.flatMap(
    buffer => filterBufferByName(buffer, name));
}

function filterBufferByName(buffer, name) {
  if (name === undefined) {
    return buffer;
  }
  return buffer.filter(it => it.name === name);
}

function bufferUserTiming(entry) {
  const entryType = entry.entryType;
  if (!kUserTimingEntryTypes.includes(entryType)) {
    return;
  }
  const buffer = getEntryBuffer(entryType);
  buffer.push(entry);
}

function bufferResourceTiming(entry) {
  const resourceTimingBuffer = getEntryBuffer('resource');
  if (resourceTimingBuffer.length < resourceTimingBufferSizeLimit && !resourceTimingBufferFullPending) {
    resourceTimingBuffer.push(entry);
    return;
  }

  if (!resourceTimingBufferFullPending) {
    resourceTimingBufferFullPending = true;
    setImmediate(() => {
      while (resourceTimingSecondaryBuffer.length > 0) {
        const excessNumberBefore = resourceTimingSecondaryBuffer.length;
        if (resourceTimingBuffer.length >= resourceTimingBufferSizeLimit) {
          dispatchBufferFull('resourcetimingbufferfull');

          const numbersToPreserve = Math.max(
            Math.min(resourceTimingBufferSizeLimit - resourceTimingBuffer.length, resourceTimingSecondaryBuffer.length),
            0
          );
          const excessNumberAfter = resourceTimingSecondaryBuffer.length - numbersToPreserve;
          for (let idx = 0; idx < numbersToPreserve; idx++) {
            resourceTimingBuffer.push(resourceTimingSecondaryBuffer[idx]);
          }

          if (excessNumberBefore <= excessNumberAfter) {
            resourceTimingSecondaryBuffer = [];
          }
          resourceTimingBufferFullPending = false;
        }
      }
    });
  }
  resourceTimingSecondaryBuffer.push(entry);
}

function dispatchBufferFull(type) {
  const globalScope = load('global/index');
  const event = new Event(type);
  trustedDispatchEvent(globalScope.performance, event);
}

wrapper.mod = {
  PerformanceObserver,
  PerformanceObserverEntryList,
  enqueue,
  clearEntriesFromBuffer,
  filterBufferMapByNameAndType,
  bufferUserTiming,
  bufferResourceTiming,
};
