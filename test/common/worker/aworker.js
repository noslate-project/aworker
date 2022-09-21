'use strict';

// Required by testharness.js
globalThis.GLOBAL = {
  isWindow() { return false; },
};

// MARK: - Code stub to be replaced at runtime.
/** __WORKER_HARNESS_CODE__ */
// MARK: END - Code stub to be replaced at runtime.

// MARK: - Code stub to be replaced at runtime.
/** __WORKER_SCRIPTS_TO_RUN */
// MARK: END - Code stub to be replaced at runtime.

add_result_callback(result => {
  console.log(`# WORKER - result: ${JSON.stringify({
    status: result.status,
    name: result.name,
    message: result.message,
    stack: result.stack,
  })}`);
});

add_completion_callback((_, status) => {
  console.log(`# WORKER - completion: ${JSON.stringify(status)}`);
});
