'use strict';

// Create a prettified stacktrace.
const ErrorToString = Error.prototype.toString; // Capture original toString.
const prepareStackTrace = (globalThis, error, callSites) => {
  const errorString = ErrorToString.call(error);

  if (callSites.length === 0) {
    return errorString;
  }
  const preparedTrace = callSites.map((t, i) => {
    let str = i !== 0 ? '\n    at ' : '';
    str = `${str}${t}`;
    return str;
  });
  return `${errorString}\n    at ${preparedTrace.join('')}`;
};

wrapper.mod = {
  prepareStackTrace,
};
