'use strict';

const options = loadBinding('aworker_options');
const process = loadBinding('process');
const {
  createExtendableEvent,
  getExtendableEventExtendedLifetimePromise,
} = load('service_worker/event');
const { trustedDispatchEvent } = load('dom/event_target');
const globalScope = load('global/index');
const { WORKER_STATE_FORK_PREPARED } = loadBinding('constants');

function main() {
  const script = load('loader/script');
  const esm = load('loader/esm');
  script.preloadScript();

  let mainFuture;
  if (options.snapshot_blob) {
    const mksnapshot = load('loader/mksnapshot');
    mainFuture = mksnapshot.runDeserializeMain();
  } else if (options.module) {
    mainFuture = esm.runMain();
  } else {
    mainFuture = script.runMain();
  }

  Promise.resolve(mainFuture)
    .then(function afterPreForkMainEntry() {
      const serializedEvent = createExtendableEvent('serialize');
      trustedDispatchEvent(globalScope, serializedEvent);
      return getExtendableEventExtendedLifetimePromise(serializedEvent);
    })
    .then(() => {
      process.setWorkerState(WORKER_STATE_FORK_PREPARED);
    });
}

main();
