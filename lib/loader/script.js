'use strict';

const options = loadBinding('aworker_options');
const file = load('file');
const path = load('path');
const { Script, defaultContext, ExecutionFlags } = load('js');
const { traceEvent } = load('console/debuglog');
const process = loadBinding('process');

function maybeCacheSourceMap(filename, content) {
  if (options.enable_source_maps !== true) {
    return;
  }

  load('source_maps/source_map_cache').maybeCacheSourceMap(filename, content);
}

function runMain() {
  if (!options.has_script_filename) {
    throw new Error('No script filename specified in command line arguments');
  }
  if (options.eval) {
    evaluate(options.script_filename, options.inspect_brk ? ExecutionFlags.kBreakOnFirstLine : ExecutionFlags.kNone);
  } else {
    loadScript(path.resolve(options.script_filename), options.inspect_brk ? ExecutionFlags.kBreakOnFirstLine : ExecutionFlags.kNone);
  }
}

function preloadScript() {
  if (!options.has_preload_script) {
    return;
  }
  loadScript(options.preload_script, ExecutionFlags.kNone);
}

function loadScript(filename, executionFlags) {
  const content = traceEvent('aworker.ReadScriptFile', () => file.readFile(filename, 'utf8'));
  traceEvent('aworker.maybeCacheSourceMap', () => maybeCacheSourceMap(filename, content));
  const script = traceEvent('aworker.ParseScript', () => new Script(content, { filename }));

  process.startLoopLatencyWatchdogIfNeeded();
  traceEvent('aworker.EvaluateScript', () => defaultContext().execute(script, 0, executionFlags));
}

function evaluate(content, executionFlags) {
  maybeCacheSourceMap('eval_machine', content);
  const script = new Script(content, { filename: 'eval_machine' });

  process.startLoopLatencyWatchdogIfNeeded();
  defaultContext().execute(script, 0, executionFlags);
}

wrapper.mod = {
  runMain,
  preloadScript,
};
