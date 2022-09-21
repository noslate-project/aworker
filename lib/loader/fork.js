'use strict';

const {
  createExtendableEvent,
  getExtendableEventExtendedLifetimePromise,
} = load('service_worker/event');
const { trustedDispatchEvent } = load('dom/event_target');
const globalScope = load('global/index');

function runChildMain() {
  const deserializeEvent = createExtendableEvent('deserialize');
  trustedDispatchEvent(globalScope, deserializeEvent);
  return getExtendableEventExtendedLifetimePromise(deserializeEvent);
}

wrapper.mod = {
  runChildMain,
};
