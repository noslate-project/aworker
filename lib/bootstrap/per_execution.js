'use strict';

const { EventTarget } = load('dom/event_target');
const options = loadBinding('aworker_options');
const { Location } = load('dom/location');
const { defineGlobalProperties } = load('global/utils');
const debuglog = load('console/debuglog');

debuglog.resolveCategories();
// TODO: find a proper way to re-install globalThis to WeakMaps.
// Re-applying EventTarget Mixin to get globalThis to be recognized
// as a EventTarget.
EventTarget.apply(globalThis);

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
  const location = new Location(options.has_location ? options.location : 'file://' + options.script_filename);

  defineGlobalProperties(globalThis, {
    location,
  });
}

function setupAgentDataChannel() {
  // TODO(chengzhong.wcz): explicit `setupAgent()` in `internal/agent_channel`;
  load('agent_channel');
}
