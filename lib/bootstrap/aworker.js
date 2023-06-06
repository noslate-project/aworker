'use strict';

const process = loadBinding('process');

function setupBridges() {
  const execution = load('process/execution');
  const taskQueue = load('task_queue');
  const { DOMException } = load('dom/exception');
  for (const key of [
    'tickTaskQueue',
    'asyncWrapInitFunction',
    'callbackTrampolineFunction',
  ]) {
    process[key] = taskQueue[key];
  }

  for (const key of [
    'emitUncaughtException',
    'emitPromiseRejection',
    'runCleanupHooks',
  ]) {
    process[key] = execution[key];
  }

  process.prepareStackTrace = execution.prepareStackTrace;
  process.domExceptionConstructor = DOMException;
}

setupBridges();
load('global/index');
