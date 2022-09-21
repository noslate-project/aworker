'use strict';

const options = loadBinding('aworker_options');
const file = load('file');
const path = load('path');
const { Script, defaultContext, ExecutionFlags } = load('js');
const {
  createExtendableEvent,
  getExtendableEventExtendedLifetimePromise,
} = load('service_worker/event');
const { trustedDispatchEvent } = load('dom/event_target');
const script = load('loader/script');
const process = loadBinding('process');
const { console } = load('console');
const globalScope = load('global/index');
const { WORKER_STATE_SERIALIZED, WORKER_STATE_DESERIALIZED } = loadBinding('constants');

function runMain() {
  if (!options.has_script_filename) {
    throw new Error('No script filename specified in command line arguments');
  }
  loadScript(path.resolve(options.script_filename), ExecutionFlags.kNone);

  const serializedEvent = createExtendableEvent('serialize');
  trustedDispatchEvent(globalScope, serializedEvent);
  return getExtendableEventExtendedLifetimePromise(serializedEvent)
    .then(() => {
      process.setWorkerState(WORKER_STATE_SERIALIZED);
    });
}

function loadScript(filename, executionFlags) {
  const content = file.readFile(filename, 'utf8');
  const script = new Script(content, { filename });
  defaultContext().execute(script, 0, executionFlags);
}

function runDeserializeMain() {
  if (!process.externalSnapshotLoaded) {
    if (!options.has_script_filename) {
      throw new Error('External startup snapshot not loaded and no fallback available');
    }
    console.error('External startup snapshot not loaded, fallback to script mode.');
    script.runMain();
    return;
  }

  const deserializeEvent = createExtendableEvent('deserialize');
  trustedDispatchEvent(globalScope, deserializeEvent);
  return getExtendableEventExtendedLifetimePromise(deserializeEvent)
    .then(() => {
      process.setWorkerState(WORKER_STATE_DESERIALIZED);
    });
}

wrapper.mod = {
  runMain,
  runDeserializeMain,
};
