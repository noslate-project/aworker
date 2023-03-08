'use strict';

const { InternalPerformanceEntry, PerformanceEntry } = load('performance/performance_entry');
const { now } = load('performance/utils');
const { bufferUserTiming } = load('performance/observer');

const { DOMException } = load('dom/exception');
const { structuredClone } = loadBinding('serdes');

const markTimings = new Map();

function getMark(name) {
  if (name === undefined) return;
  if (typeof name === 'number') {
    if (name < 0) { throw new TypeError(`Invalid timestamp ${name}`); }
    return name;
  }
  name = `${name}`;
  const ts = markTimings.get(name);
  if (ts === undefined) { throw new DOMException(`The "${name}" performance mark has not been set`, 'SyntaxError'); }
  return ts;
}

class PerformanceMark {
  constructor(name, options) {
    name = `${name}`;
    options = options ?? {};
    if (typeof options !== 'object') {
      throw new TypeError();
    }
    const startTime = options.startTime ?? now();
    if (typeof startTime !== 'number') {
      throw new TypeError();
    }
    if (startTime < 0) { throw new TypeError(`Invalid timestamp ${startTime}`); }
    markTimings.set(name, startTime);

    const detail = options.detail != null ?
      structuredClone(options.detail) :
      null;

    InternalPerformanceEntry.call(this, name, 'mark', startTime, 0, detail);
  }

  get [Symbol.toStringTag]() {
    return 'PerformanceMark';
  }
}

Object.setPrototypeOf(PerformanceMark.prototype, PerformanceEntry.prototype);
Object.setPrototypeOf(PerformanceMark, PerformanceEntry);

class PerformanceMeasure extends PerformanceEntry {
  constructor() {
    throw new TypeError('Illegal constructor');
  }

  get [Symbol.toStringTag]() {
    return 'PerformanceMeasure';
  }
}

function InternalPerformanceMeasure(name, start, duration, detail) {
  InternalPerformanceEntry.call(this, name, 'measure', start, duration, detail);
  Object.setPrototypeOf(this, PerformanceMeasure.prototype);
}

function mark(name, options) {
  const mark = new PerformanceMark(name, options);
  bufferUserTiming(mark);
  return mark;
}

function calculateStartDuration(startOrMeasureOptions, endMark) {
  startOrMeasureOptions = startOrMeasureOptions ?? 0;
  let start;
  let end;
  let duration;
  let optionsValid = false;
  if (typeof startOrMeasureOptions === 'object') {
    ({ start, end, duration } = startOrMeasureOptions);
    optionsValid = start !== undefined || end !== undefined || duration !== undefined;
  }
  if (optionsValid) {
    if (endMark !== undefined) {
      throw new TypeError(
        'endMark must not be specified');
    }
    if (start === undefined && end === undefined) {
      throw new TypeError(
        'One of options.start or options.end is required');
    }
    if (start !== undefined &&
      end !== undefined &&
      duration !== undefined) {
      throw new TypeError(
        'Must not have options.start, options.end, and ' +
        'options.duration specified');
    }
  }

  if (endMark !== undefined) {
    end = getMark(endMark);
  } else if (optionsValid && end !== undefined) {
    end = getMark(end);
  } else if (optionsValid &&
    start !== undefined &&
    duration !== undefined) {
    end = getMark(start) +
      getMark(duration);
  } else {
    end = now();
  }

  if (optionsValid && start !== undefined) {
    start = getMark(start);
  } else if (optionsValid &&
    duration !== undefined &&
    startOrMeasureOptions.end !== undefined) {
    start = end -
      getMark(duration);
  } else if (typeof startOrMeasureOptions === 'string') {
    start = getMark(startOrMeasureOptions);
  } else {
    start = 0;
  }

  duration = end - start;
  return { start, duration };
}

function measure(name, startOrMeasureOptions, endMark) {
  if (typeof name !== 'string') {
    throw new TypeError();
  }
  const {
    start,
    duration,
  } = calculateStartDuration(startOrMeasureOptions, endMark);
  let detail = startOrMeasureOptions?.detail;
  detail = detail != null ? structuredClone(detail) : null;
  const measure = new InternalPerformanceMeasure(name, start, duration, detail);
  bufferUserTiming(measure);
  return measure;
}

function clearMarkTimings(name) {
  if (name !== undefined) {
    name = `${name}`;
    markTimings.delete(name);
    return;
  }
  markTimings.clear();
}

wrapper.mod = {
  PerformanceMark,
  PerformanceMeasure,
  clearMarkTimings,
  mark,
  measure,
};
