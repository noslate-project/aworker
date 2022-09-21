'use strict';

const { ContextWrap, ScriptWrap, ExecutionFlags } = loadBinding('contextify');
const {
  isUint8Array,
} = load('types');

let unnamedContextCount = 0;

const defaultScriptOptions = {
  filename: '',
  lineOffset: 0,
  columnOffset: 0,
};

class Script extends ScriptWrap {
  constructor(code, options = { filename: '', lineOffset: 0, columnOffset: 0 }) {
    options = Object.assign({}, defaultScriptOptions, options);
    let { filename, lineOffset, columnOffset, cachedData } = options;

    if (typeof filename !== 'string') {
      throw new TypeError('options.filename should be a string');
    }

    if (typeof lineOffset !== 'number' || isNaN(lineOffset)) {
      throw new TypeError('options.lineOffset should be a valid number');
    }

    if (typeof columnOffset !== 'number' || isNaN(columnOffset)) {
      throw new TypeError('options.columnOffset should be a valid number');
    }

    if (cachedData != null && !isUint8Array(cachedData)) {
      throw new TypeError('options.columnOffset should be a Uint8Array');
    }

    if (filename === '') {
      filename = 'aworker.js.<anonymous>';
    }

    super(code, filename, lineOffset, columnOffset, cachedData);

    Object.defineProperties(this, {
      filename: {
        value: filename,
        configurable: false,
        writable: false,
        enumerable: true,
      },
      lineOffset: {
        value: lineOffset,
        configurable: false,
        writable: false,
        enumerable: true,
      },
      columnOffset: {
        value: columnOffset,
        configurable: false,
        writable: false,
        enumerable: true,
      },
    });
  }
}

class Context extends ContextWrap {
  constructor(sandbox, options = {}) {
    if (sandbox === null || typeof sandbox !== 'object') {
      throw new TypeError('Sandbox should not be `null` or `undefined`');
    }

    options = Object.assign({
      origin: '',
    }, options);

    if (options.name === undefined || options.name === null) {
      options.name = `unnamed ${unnamedContextCount++}`;
    }

    super(sandbox, options.name, options.origin);

    Object.defineProperties(this, {
      name: {
        value: options.name,
        writable: false,
        enumerable: true,
      },
      globalThis: {
        get: () => {
          return super.globalThis();
        },
        enumerable: true,
        configurable: false,
      },
    });
  }

  execute(script, options = {}) {
    if (typeof script === 'string') {
      script = new Script(script);
    }

    if (!(script instanceof Script)) {
      throw new TypeError('script should be a string or a Script instance');
    }

    options = Object.assign({
      timeout: 0,
    }, options);

    if (options.timeout === null || options.timeout === undefined) {
      options.timeout = 0;
    }

    if (typeof options.timeout !== 'number') {
      throw new TypeError('options.timeout should be a number');
    }

    if (options.timeout < 0) {
      throw new RangeError('options.timeout should greate than or equal to 0');
    }

    return super.execute(script, options.timeout, ExecutionFlags.kNone);
  }
}

function defaultContext() {
  return new ContextWrap();
}

wrapper.mod = {
  Context,
  Script,
  defaultContext,
  ExecutionFlags,
  __exposed__: {
    Context,
    Script,
  },
};
