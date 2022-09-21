'use strict';

const {
  COMPRESS_TYPE,
  FLUSH_MODE,
  LEVEL,
  STRATEGY,
  UNCOMPRESS_TYPE,
  UnzipWrapper,
  WINDOWBITS,
  ZipWrapper,
} = loadBinding('zlib');

const { createDeferred, toArrayBuffer } = load('utils');
const { TransformStream } = load('streams');

const CORE = Symbol('ZlibWrapper::core');
const defaultOptions = {
  dictionary: null, // TODO(kaidi.zkd): dictionary support
  flush: FLUSH_MODE.Z_NO_FLUSH,
  windowBits: WINDOWBITS.Z_DEFAULT_WINDOWBITS,

  // compress only
  level: LEVEL.Z_DEFAULT_LEVEL,
  memLevel: LEVEL.Z_DEFAULT_MEMLEVEL,
  strategy: STRATEGY.Z_DEFAULT_STRATEGY,
};

function checkRange(name, value, min, max) {
  if (typeof value !== 'number') throw new RangeError(`${name} should be a number.`);
  if (isNaN(value)) throw new RangeError(`${name} cannot be a NaN.`);
  if (value >= min && value <= max) return;
  throw new Error(`${name} (${value}) was supposed to between ${min} and ${max}.`);
}

class ZlibWrapper extends TransformStream {
  #options;
  #flush;

  constructor(type, options, overrideTransformer = {}) {
    const transformer = {
      transform: (...args) => this.#transform(...args),
      ...overrideTransformer,
    };

    super(transformer);

    this.#options = options;
    this.#flush = options.flush;
  }

  #transform(chunk, controller) {
    if (this[CORE].isDone()) {
      throw new Error(`${this[CORE].constructor.name.replace('Wrapper', '')} already done.`);
    }

    const meta = toArrayBuffer(chunk);

    const { buff, offset, length } = meta;
    const { resolve, reject, promise } = createDeferred();

    this[CORE].push(
      // slice the `buff` because `buff` will be detached in the C++ layer.
      buff.slice(offset, offset + length), // buffer
      0, // offset
      length,
      this.#flush,
      (err, result) => {
        if (err) {
          return reject(err);
        }

        if (Array.isArray(result)) {
          if (result[0].byteLength) controller.enqueue(result[0]);
          if (result[1].byteLength) controller.enqueue(result[1]);
          return resolve();
        }

        if (result.byteLength) controller.enqueue(result);
        resolve();
      });

    return promise;
  }
}

class Zip extends ZlibWrapper {
  constructor(type, options = {}) {
    checkRange('type', type, COMPRESS_TYPE.GZIP, COMPRESS_TYPE.DEFLATE);

    const _options = {
      ...defaultOptions,
      ...options,
    };

    {
      const min = WINDOWBITS.Z_MIN_WINDOWBITS + (type === COMPRESS_TYPE.GZIP ? 1 : 0);
      checkRange(
        'options.windowBits',
        _options.windowBits,
        min,
        WINDOWBITS.Z_MAX_WINDOWBITS);
    }

    checkRange('options.level', _options.level, LEVEL.Z_MIN_LEVEL, LEVEL.Z_MAX_LEVEL);
    checkRange('options.memLevel', _options.memLevel, LEVEL.Z_MIN_MEMLEVEL, LEVEL.Z_MAX_MEMLEVEL);
    checkRange('options.strategy', _options.strategy, STRATEGY.Z_DEFAULT_STRATEGY, STRATEGY.Z_FIXED);

    super(type, _options, {
      flush: controller => this.#flush(controller),
    });

    // type, sync, windowBits, [dictionary: TODO, ] options
    this[CORE] = new ZipWrapper(
      type,
      false, // isSync: stream mode always false
      _options.windowBits,
      _options);
  }

  async #flush(controller) {
    const { resolve, reject, promise } = createDeferred();

    // Push an empty `ArrayBuffer` with `FLUSH_MODE.Z_FINISH` to end the compress context.
    this[CORE].push(new ArrayBuffer(0), 0, 0, FLUSH_MODE.Z_FINISH, (err, result) => {
      if (err) {
        return reject(err);
      }

      if (result.byteLength) controller.enqueue(result);

      // Note that there is no need to call controller.terminate() inside flush(); the stream is already in the process
      // of successfully closing down, and terminating it would be counterproductive.
      //
      // Refs: https://streams.spec.whatwg.org/#dom-transformer-flush

      return resolve();
    });

    return promise;
  }
}

class Unzip extends ZlibWrapper {
  constructor(type, options = {}) {
    checkRange('type', type, UNCOMPRESS_TYPE.GUNZIP, UNCOMPRESS_TYPE.UNZIP);

    const _options = {
      ...defaultOptions,
      ...options,
    };

    if (options.windowBits === 0 || options.windowBits === null) {
      _options.windowBits = 0;
    } else {
      checkRange(
        'options.windowBits',
        _options.windowBits,
        WINDOWBITS.Z_MIN_WINDOWBITS,
        WINDOWBITS.Z_MAX_WINDOWBITS);
    }

    checkRange(
      'options.flush',
      _options.flush,
      FLUSH_MODE.Z_NO_FLUSH,
      FLUSH_MODE.Z_BLOCK);

    super(type, _options, {
      flush: controller => this.#flush(controller),
    });

    // type, sync, windowBits, [dictionary: TODO, ]
    this[CORE] = new UnzipWrapper(
      type,
      false, // isSync: stream mode always false
      _options.windowBits);
  }

  async #flush() {
    if (!this[CORE].isDone()) {
      throw new Error('Uncompression is not completed yet.');
    }
  }
}

mod.COMPRESS_TYPE = COMPRESS_TYPE;
mod.UNCOMPRESS_TYPE = UNCOMPRESS_TYPE;
mod.FLUSH_MODE = FLUSH_MODE;
mod.STRATEGY = STRATEGY;
mod.Unzip = Unzip;
mod.Zip = Zip;
