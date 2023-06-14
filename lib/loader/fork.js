'use strict';

const {
  createExtendableEvent,
  getExtendableEventExtendedLifetimePromise,
} = load('service_worker/event');
const { trustedDispatchEvent } = load('dom/event_target');
const globalScope = load('global/index');
const process = loadBinding('process');

function runChildMain() {
  const deserializeEvent = createExtendableEvent('deserialize');

  process.loopLatencyWatchdogPrologue();
  trustedDispatchEvent(globalScope, deserializeEvent);
  process.loopLatencyWatchdogEpilogue();

  return getExtendableEventExtendedLifetimePromise(deserializeEvent);
}

wrapper.mod = {
  runChildMain,
};
