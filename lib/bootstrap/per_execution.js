'use strict';

const options = loadBinding('aworker_options');
const { Location } = load('dom/location');
const { defineGlobalProperties } = load('global/utils');
const debuglog = load('console/debuglog');

debuglog.resolveCategories();

setupPromiseHooks();
setupExposeInternals();
setupPerExecutionGlobals();
setupAgentDataChannel();

function setupPromiseHooks() {
  // Promise hooks can not be snapshotted.
  const asyncWrap = loadBinding('async_wrap');
  const taskQueue = load('task_queue');

  asyncWrap.setJSPromiseHooks(
    taskQueue.promiseInitHook,
    taskQueue.promiseBeforeHook,
    taskQueue.promiseAfterHook
  );
}

function setupExposeInternals() {
  if (options.expose_internals !== true) {
    return;
  }

  defineGlobalProperties(globalThis, {
    load,
    loadBinding,
  });
}

function setupPerExecutionGlobals() {
  let locationStr;
  if (options.has_location) {
    locationStr = options.location;
  } else if (options.eval) {
    locationStr = 'aworker://eval-machine';
  } else {
    locationStr = 'file://' + options.script_filename;
  }
  const location = new Location(locationStr);

  defineGlobalProperties(globalThis, {
    location,
  });
}

function setupAgentDataChannel() {
  // TODO(chengzhong.wcz): explicit `setupAgent()` in `internal/agent_channel`;
  load('agent_channel');
}
