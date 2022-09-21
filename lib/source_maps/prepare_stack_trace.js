'use strict';

const { findSourceMap } = load('source_maps/source_map_cache');
const debug = load('console/debuglog').debuglog('prepare_stack_trace');

// Create a prettified stacktrace, inserting context from source maps
// if possible.
const ErrorToString = Error.prototype.toString; // Capture original toString.
const prepareStackTrace = (globalThis, error, callSites) => {
  const errorString = ErrorToString.call(error);

  if (callSites.length === 0) {
    return errorString;
  }
  const preparedTrace = callSites.map((t, i) => {
    let str = i !== 0 ? '\n    at ' : '';
    str = `${str}${t}`;
    try {
      const sm = findSourceMap(t.getFileName(), error);
      if (sm) {
        // Source Map V3 lines/columns use zero-based offsets whereas, in
        // stack traces, they start at 1/1.
        const {
          originalLine,
          originalColumn,
          originalSource,
        } = sm.findEntry(t.getLineNumber() - 1, t.getColumnNumber() - 1);
        if (originalSource && originalLine !== undefined &&
            originalColumn !== undefined) {
          str +=
`\n        -> ${originalSource.replace('file://', '')}:${originalLine + 1}:${originalColumn + 1}`;
        }
      }
    } catch (err) {
      debug(err.stack);
    }
    return str;
  });
  return `${errorString}\n    at ${preparedTrace.join('')}`;
};

wrapper.mod = {
  prepareStackTrace,
};
