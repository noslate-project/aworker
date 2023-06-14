'use strict';

const {
  ModuleWrap,
  setImportModuleDynamicallyCallback,
  setInitializeImportMetaObjectCallback,
} = loadBinding('module_wrap');
const options = loadBinding('aworker_options');
const file = load('file');
const path = load('path');
const { defaultContext, ExecutionFlags } = load('js');
const debug = load('console/debuglog').debuglog('esm_loader');
const process = loadBinding('process');

function maybeCacheSourceMap(filename, content) {
  if (options.enable_source_maps !== true) {
    return;
  }

  load('source_maps/source_map_cache').maybeCacheSourceMap(filename, content);
}

async function runMain() {
  if (!options.has_script_filename) {
    throw new Error('No script filename specified in command line arguments');
  }
  const module = await constructAndInstantiateModule(path.resolve(options.script_filename));

  process.loopLatencyWatchdogPrologue();
  const ret = defaultContext().execute(module, 0, (options.inspect_brk ? ExecutionFlags.kBreakOnFirstLine : ExecutionFlags.kNone) | ExecutionFlags.kModule);
  process.loopLatencyWatchdogEpilogue();
  return ret;
}

// TODO(chengzhong.wcz): implement https://whatwg.github.io/loader/
const moduleMap = new Map();
const moduleLinkingSet = new Set();

async function constructModule(filename) {
  debug('construct module', filename);
  // TODO(chengzhong.wcz): readFileAsync
  const content = file.readFile(filename, 'utf8');
  maybeCacheSourceMap(filename, content);

  let module;
  if (!moduleMap.has(filename)) {
    module = new ModuleWrap(content, filename, 0, 0);
    module.filename = filename;
    moduleMap.set(filename, module);
  } else {
    module = moduleMap.get(filename);
  }

  return module;
}

async function linkModule(module) {
  if (moduleLinkingSet.has(module)) {
    return;
  }
  moduleLinkingSet.add(module);
  const modules = await Promise.all(module.link(resolver));
  await Promise.all(modules.map(linkModule));
  module.instantiate();
}

async function constructAndInstantiateModule(filename) {
  const module = await constructModule(filename);
  await linkModule(module);
  return module;
}

function resolveSpecifier(referrerResourceName, specifier) {
  let resolved;
  // Allow '../../foo.js', './foo.js'
  if (specifier.startsWith('./') || specifier.startsWith('../')) {
    resolved = path.resolve(path.dirname(referrerResourceName), specifier);
  } else {
    // Unsupported specifier.
    throw new Error(`Cannot find package '${specifier}' imported from ${referrerResourceName}`);
  }
  return resolved;
}

async function resolver(referrer, specifier) {
  const referrerResourceName = referrer.filename;
  const resolved = resolveSpecifier(referrerResourceName, specifier);
  debug('import from module(%s) with request(%s, resolved: %s)', referrerResourceName, specifier, resolved);
  return constructModule(resolved);
}

async function dynamicImport(referrerResourceName, specifier) {
  const resolved = resolveSpecifier(referrerResourceName, specifier);
  debug('dynamic import from module(%s) with request(%s, resolved: %s)', referrerResourceName, specifier, resolved);
  const mod = await constructAndInstantiateModule(resolved);
  defaultContext().execute(mod, 0, ExecutionFlags.kModule);
  return mod.getNamespace();
}

async function initializeImportMeta(module, meta) {
  // TODO(chengzhong.wcz): maybe we can initialize the module with filename set
  // the this url. However that requires the resolver to understand url, which
  // seems quite redundant at the time.
  meta.url = 'file://' + module.filename;
}

setImportModuleDynamicallyCallback(dynamicImport);
setInitializeImportMetaObjectCallback(initializeImportMeta);

wrapper.mod = {
  runMain,
};
