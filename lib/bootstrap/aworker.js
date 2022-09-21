'use strict';

const process = loadBinding('process');

function setupBridges() {
  const execution = load('process/execution');
  const taskQueue = load('task_queue');
  const { DOMException } = load('dom/exception');
  for (const key of [
    'tickTaskQueue',
    'asyncWrapInitFunction',
    'asyncWrapBeforeFunction',
    'asyncWrapAfterFunction',
  ]) {
    process[key] = typeof taskQueue[key] === 'function' ?
      taskQueue[key].bind(globalThis) :
      taskQueue[key];
  }

  for (const key of [
    'emitUncaughtException',
    'emitPromiseRejection',
    'runCleanupHooks',
  ]) {
    process[key] = typeof execution[key] === 'function' ?
      execution[key].bind(globalThis) :
      execution[key];
  }

  process.prepareStackTrace = execution.prepareStackTrace;
  process.domExceptionConstructor = DOMException;
}

setupBridges();
load('global/index');
