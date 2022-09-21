'use strict';

const process = loadBinding('process');

const userAgent = 'aworker';

// https://html.spec.whatwg.org/multipage/workers.html#workernavigator
class WorkerNavigator {
  // https://html.spec.whatwg.org/multipage/system-state.html#navigatorid
  get appCodeName() {
    return 'Mozilla';
  }

  get appName() {
    return 'Netscape';
  }

  get appVersion() {
    return process.version;
  }

  get platform() {
    return process.platform;
  }

  get product() {
    return 'Gecko';
  }

  get userAgent() {
    return userAgent;
  }

  // https://html.spec.whatwg.org/multipage/system-state.html#navigatorlanguage
  get language() {
    return 'en-US';
  }

  get languages() {
    return [ 'en-US' ];
  }

  // https://html.spec.whatwg.org/multipage/system-state.html#navigatoronline
  get onLine() {
    return true;
  }

  // https://html.spec.whatwg.org/multipage/workers.html#navigatorconcurrenthardware
  get hardwareConcurrency() {
    return 1;
  }
}

const navigator = new WorkerNavigator();

wrapper.mod = {
  WorkerNavigator,
  navigator,
  userAgent,
};
