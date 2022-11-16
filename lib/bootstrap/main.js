'use strict';

const options = loadBinding('aworker_options');
const process = loadBinding('process');
const { dispatchInstallEvent, installSigtermHandler } = load('service_worker/index');
const { STARTUP_FORK_MODE_FORK_USERLAND } = loadBinding('constants');


function main() {
  if (options.build_snapshot) {
    const mksnapshot = load('loader/mksnapshot');
    mksnapshot.runMain();
    return;
  }

  const script = load('loader/script');
  const esm = load('loader/esm');
  script.preloadScript();

  let mainFuture;
  if (options.snapshot_blob) {
    const mksnapshot = load('loader/mksnapshot');
    mainFuture = mksnapshot.runDeserializeMain();
  } else if (process.startupForkMode === STARTUP_FORK_MODE_FORK_USERLAND) {
    const forkLoader = load('loader/fork');
    mainFuture = forkLoader.runChildMain();
  } else if (options.module) {
    mainFuture = esm.runMain();
  } else {
    mainFuture = script.runMain();
  }

  Promise.resolve(mainFuture)
    .then(function afterMainEntry() {
      return dispatchInstallEvent();
    }).then(() => {
      return installSigtermHandler();
    });
}

main();
