'use strict';
// See https://sourcemaps.info/spec.html for SourceMap V3 specification.

const options = loadBinding('aworker_options');
const execution = load('process/execution');
const { atob } = load('bytes');
const { URL } = load('url');
const { fileURLToPath, normalizeReferrerURL } = load('utils');
const { dirname, resolve } = load('path');
const { debug } = load('console/debuglog');
const TAG = 'source_map_cache';
let SourceMap;

// The cache is not exposed to users, so we can use a Map keyed
// on filenames.
const esmSourceMapCache = new Map();

let enableSourceMaps;
function getSourceMapsEnabled() {
  if (enableSourceMaps === undefined) {
    enableSourceMaps = options.enable_source_maps;
    if (enableSourceMaps) {
      execution.setPrepareStackTraceCallback(load('source_maps/prepare_stack_trace').prepareStackTrace);
    }
  }
  return enableSourceMaps;
}

// TODO: V8 Coverage Support;
const v8Coverage = false;
function maybeCacheSourceMap(filename, content) {
  if (!(v8Coverage || getSourceMapsEnabled())) return;
  let basePath;
  try {
    filename = normalizeReferrerURL(filename);
    basePath = dirname(fileURLToPath(filename));
  } catch (err) {
    // This is most likely an [eval]-wrapper, which is currently not
    // supported.
    return;
  }

  const match = content.match(/\/[*/]#\s+sourceMappingURL=(?<sourceMappingURL>[^\s]+)/);
  if (match) {
    const data = dataFromUrl(basePath, match.groups.sourceMappingURL);
    const url = data ? null : match.groups.sourceMappingURL;

    esmSourceMapCache.set(filename, {
      lineLengths: lineLengths(content),
      data,
      url,
    });
  }
}

function dataFromUrl(basePath, sourceMappingURL) {
  try {
    const url = new URL(sourceMappingURL);
    switch (url.protocol) {
      case 'data:':
        return sourceMapFromDataUrl(basePath, url.pathname);
      default:
        debug(TAG, `unknown protocol ${url.protocol}`);
        return null;
    }
  } catch (err) {
    debug(TAG, err.stack);
    // If no scheme is present, we assume we are dealing with a file path.
    const sourceMapFile = resolve(basePath, sourceMappingURL);
    return sourceMapFromFile(sourceMapFile);
  }
}

// Cache the length of each line in the file that a source map was extracted
// from. This allows translation from byte offset V8 coverage reports,
// to line/column offset Source Map V3.
function lineLengths(content) {
  // We purposefully keep \r as part of the line-length calculation, in
  // cases where there is a \r\n separator, so that this can be taken into
  // account in coverage calculations.
  return content.split(/\n|\u2028|\u2029/).map(line => {
    return line.length;
  });
}

function sourceMapFromFile() {
  return null;
}

// data:[<mediatype>][;base64],<data> see:
// https://tools.ietf.org/html/rfc2397#section-2
function sourceMapFromDataUrl(basePath, url) {
  const [ format, data ] = url.split(',');
  const splitFormat = format.split(';');
  const contentType = splitFormat[0];
  const base64 = splitFormat[splitFormat.length - 1] === 'base64';
  if (contentType === 'application/json') {
    // TODO: Replace with buffer decode base64
    const decodedData = base64 ? atob(data) : data;
    try {
      const parsedData = JSON.parse(decodedData);
      return sourcesToAbsolute(basePath, parsedData);
    } catch (err) {
      debug(TAG, err.stack);
      return null;
    }
  } else {
    debug(TAG, `unknown content-type ${contentType}`);
    return null;
  }
}

// If the sources are not absolute URLs after prepending of the "sourceRoot",
// the sources are resolved relative to the SourceMap (like resolving script
// src in a html document).
function sourcesToAbsolute(base, data) {
  data.sources = data.sources.map(source => {
    source = (data.sourceRoot || '') + source;
    if (!/^[\\/]/.test(source[0])) {
      source = resolve(base, source);
    }
    if (!source.startsWith('file://')) source = `file://${source}`;
    return source;
  });
  // The sources array is now resolved to absolute URLs, sourceRoot should
  // be updated to noop.
  data.sourceRoot = '';
  return data;
}

// WARNING: The `sourceMapCacheToObject` run during shutdown. In particular,
// they also run when workers are terminated, making it important that they
// do not call out to any user-provided code, including built-in prototypes
// that might have been tampered with.

// Get serialized representation of source-map cache, this is used
// to persist a cache of source-maps to disk when v8 coverage is enabled.
function sourceMapCacheToObject() {
  const obj = Object.create(null);

  const it = esmSourceMapCache.entries();
  let entry;
  while (!(entry = it.next()).done) {
    const k = entry.value[0];
    const v = entry.value[1];
    obj[k] = v;
  }

  if (Object.keys(obj).length === 0) {
    return undefined;
  }
  return obj;
}

// Attempt to lookup a source map attached to a file URI.
function findSourceMap(uri) {
  if (!SourceMap) {
    SourceMap = load('source_maps/source_map').SourceMap;
  }
  if (!uri.startsWith('file://')) uri = normalizeReferrerURL(uri);
  const sourceMap = esmSourceMapCache.get(uri);
  if (sourceMap && sourceMap.data) {
    return new SourceMap(sourceMap.data);
  }
  return undefined;
}

wrapper.mod = {
  findSourceMap,
  maybeCacheSourceMap,
  sourceMapCacheToObject,
};
