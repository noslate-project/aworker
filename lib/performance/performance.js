'use strict';

const {
  EventTarget,
} = load('dom/event_target');

const { now, eventLoopUtilization } = load('performance/utils');

const {
  mark,
  measure,
  clearMarkTimings,
} = load('performance/user_timing');
const aworkerTiming = load('performance/aworker_timing');
const {
  clearEntriesFromBuffer,
  filterBufferMapByNameAndType,
} = load('performance/observer');
const { formatWithOptions, customInspectSymbol: kInspect } = load('console/format');

const { getTimeOriginTimeStamp } = loadBinding('perf');

class Performance extends EventTarget {
  constructor() {
    throw new TypeError('Illegal constructor');
  }

  [kInspect](depth, options) {
    if (depth < 0) return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `Performance ${formatWithOptions(opts, {
      aworkerTiming: { ...this.aworkerTiming },
      timeOrigin: this.timeOrigin,
    })}`;
  }
}

function toJSON() {
  return {
    aworkerTiming: { ...this.aworkerTiming },
    timeOrigin: this.timeOrigin,
    eventLoopUtilization: this.eventLoopUtilization(),
  };
}

function clearMarks(name) {
  if (name !== undefined) {
    name = `${name}`;
  }
  clearMarkTimings(name);
  clearEntriesFromBuffer('mark', name);
}

function clearMeasures(name) {
  if (name !== undefined) {
    name = `${name}`;
  }
  clearEntriesFromBuffer('measure', name);
}

function clearResourceTimings(name) {
  if (name !== undefined) {
    name = `${name}`;
  }
  clearEntriesFromBuffer('resource', name);
}

function getEntries() {
  return filterBufferMapByNameAndType();
}

function getEntriesByName(name) {
  if (name !== undefined) {
    name = `${name}`;
  }
  return filterBufferMapByNameAndType(name, undefined);
}

function getEntriesByType(type) {
  if (type !== undefined) {
    type = `${type}`;
  }
  return filterBufferMapByNameAndType(undefined, type);
}

class InternalPerformance extends EventTarget { }
InternalPerformance.prototype.constructor = Performance.prototype.constructor;
Object.setPrototypeOf(InternalPerformance.prototype, Performance.prototype);

Object.defineProperties(Performance.prototype, {
  clearMarks: {
    configurable: true,
    enumerable: false,
    value: clearMarks,
  },
  clearMeasures: {
    configurable: true,
    enumerable: false,
    value: clearMeasures,
  },
  clearResourceTimings: {
    configurable: true,
    enumerable: false,
    value: clearResourceTimings,
  },
  getEntries: {
    configurable: true,
    enumerable: false,
    value: getEntries,
  },
  getEntriesByName: {
    configurable: true,
    enumerable: false,
    value: getEntriesByName,
  },
  getEntriesByType: {
    configurable: true,
    enumerable: false,
    value: getEntriesByType,
  },
  mark: {
    configurable: true,
    enumerable: false,
    value: mark,
  },
  measure: {
    configurable: true,
    enumerable: false,
    value: measure,
  },
  now: {
    configurable: true,
    enumerable: false,
    value: now,
  },
  // This should be updated during pre-execution in case
  // the process is launched from a snapshot.
  // TODO(chengzhong.wcz): we may want to warn about access to
  // this during snapshot building.
  timeOrigin: {
    configurable: true,
    enumerable: true,
    get() {
      return getTimeOriginTimeStamp();
    },
  },
  aworkerTiming: {
    configurable: true,
    enumerable: true,
    get() {
      return aworkerTiming;
    },
  },
  eventLoopUtilization: {
    configurable: true,
    enumerable: true,
    value: eventLoopUtilization,
  },
  toJSON: {
    configurable: true,
    enumerable: true,
    value: toJSON,
  },
});


const performance = new InternalPerformance();

wrapper.mod = {
  Performance,
  performance,
};
