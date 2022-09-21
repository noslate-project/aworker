'use strict';

const process = loadBinding('process');
const { formatWithOptions } = load('console/format');
const { trace } = loadBinding('tracing');
const {
  TRACE_EVENT_PHASE_BEGIN,
  TRACE_EVENT_PHASE_END,
  TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN,
  TRACE_EVENT_PHASE_NESTABLE_ASYNC_END,
} = loadBinding('constants').tracing;
const { getTimeOriginRelativeTimeStamp } = loadBinding('perf');

let enabledCategories = new Set();
function resolveCategories() {
  enabledCategories = new Set((process.env.AWORKER_DEBUG ?? '').split(',').map(it => it.toLowerCase()));
}
resolveCategories();

function getEnabled(category) {
  return enabledCategories.has(category);
}

function debug(category, format, ...args) {
  if (!getEnabled(category)) {
    return;
  }
  const content = formatWithOptions({}, `[${category.toUpperCase()}] ${new Date().toISOString()} ${format}`, ...args) + '\n';
  process.writeStdxxx(/* stderr */1, content);
}

function debuglog(category) {
  category = String(category);
  return debug.bind(null, category);
}

function traceEvent(name, fn) {
  trace(TRACE_EVENT_PHASE_BEGIN, 'aworker', name);
  const ret = fn();
  trace(TRACE_EVENT_PHASE_END, 'aworker', name);
  debug('performance', '[print time] %s: %f', name, getTimeOriginRelativeTimeStamp());
  return ret;
}

function traceAsync(category, type, id, promise) {
  trace(TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN, category, type, id);
  debug('trace', '%s - %s (%s) begin', category, type, id);
  return promise.finally(() => {
    trace(TRACE_EVENT_PHASE_NESTABLE_ASYNC_END, category, type, id);
    debug('trace', '%s - %s (%s) end', category, type, id);
  });
}

wrapper.mod = {
  resolveCategories,
  getDebuglogEnabled: getEnabled,
  debug,
  debuglog,
  traceEvent,
  traceAsync,
};
