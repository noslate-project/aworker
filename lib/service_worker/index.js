'use strict';

const {
  createExtendableEvent,
  getExtendableEventExtendedLifetimePromise,
} = load('service_worker/event');
const { trustedDispatchEvent } = load('dom/event_target');
const { debug } = load('console/debuglog');
const process = loadBinding('process');
const { WORKER_STATE_INSTALLED, WORKER_STATE_ACTIVATED, signals } = loadBinding('constants');
const { getTimeOriginRelativeTimeStamp } = loadBinding('perf');
const Signal = load('signals');

const installEvent = createExtendableEvent('install');
let resolveInstallFuture;
const installFuture = new Promise(resolve => {
  resolveInstallFuture = resolve;
}).then(() => getExtendableEventExtendedLifetimePromise(installEvent))
  .then(() => {
    process.setWorkerState(WORKER_STATE_INSTALLED);
  })
  .finally(() => {
    debug('performance', `(${getTimeOriginRelativeTimeStamp()}) install event extended promise settled`);
  });

const activateEvent = createExtendableEvent('activate');
let resolveActivateFuture;
const activateFuture = new Promise(resolve => {
  resolveActivateFuture = resolve;
}).then(() => getExtendableEventExtendedLifetimePromise(activateEvent))
  .then(() => {
    process.setWorkerState(WORKER_STATE_ACTIVATED);
  })
  .finally(() => {
    debug('performance', 'activate event extended promise settled');
  });

const uninstallEvent = createExtendableEvent('uninstall');
let resolveUninstallFuture;
const uninstallFuture = new Promise(resolve => {
  resolveUninstallFuture = resolve;
}).then(() => getExtendableEventExtendedLifetimePromise(uninstallEvent));

function dispatchInstallEvent() {
  debug('performance', `(${getTimeOriginRelativeTimeStamp()}) dispatch install event`);
  const globalScope = load('global/index');
  trustedDispatchEvent(globalScope, installEvent);
  resolveInstallFuture();
  return installFuture;
}

function installSigintHandler() {
  const signal = new Signal(signals.SIGINT);
  signal.onSignal = () => {
    dispatchUninstallEvent()
      .then(() => {
        signal.raise(signals.SIGINT);
      });
    signal.stop();
  };
  return signal;
}

function dispatchUninstallEvent() {
  debug('performance', `(${getTimeOriginRelativeTimeStamp()}) dispatch uninstall event`);
  const globalScope = load('global/index');
  trustedDispatchEvent(globalScope, uninstallEvent);
  resolveUninstallFuture();
  return uninstallFuture;
}

function dispatchActivateEvent() {
  debug('performance', 'dispatch activate event');
  const globalScope = load('global/index');
  trustedDispatchEvent(globalScope, activateEvent);
  resolveActivateFuture();
  return activateFuture;
}

wrapper.mod = {
  installFuture,
  activateFuture,
  dispatchInstallEvent,
  dispatchActivateEvent,
  installSigintHandler,
};
