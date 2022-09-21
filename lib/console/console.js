'use strict';

const { formatWithOptions, customInspectSymbol } = load('console/format');
const { consoleCall, writeStdxxx, isATTY } = loadBinding('process');

const vmConsole = globalThis.console;

const FORMAT = Symbol('Console#format');
const STD_ENUM = Symbol('Console#stdEnum');

const STD = {
  OUT: 0,
  ERR: 1,
};

class Std {
  constructor(_enum) {
    if (_enum !== STD.OUT && _enum !== STD.ERR) {
      throw new TypeError(`Can't create Std instance with ${_enum}.`);
    }

    this[STD_ENUM] = _enum;
  }

  [FORMAT](...args) {
    return formatWithOptions({
      colors: this.isTTY,
    }, ...args);
  }

  get isTTY() {
    return isATTY(this[STD_ENUM]);
  }

  output(...args) {
    const content = this[FORMAT](...args) + '\n';
    writeStdxxx(this[STD_ENUM], content);
  }
}

class Console {
  #stdout = new Std(STD.OUT);
  #stderr = new Std(STD.ERR);
  #timeLabel = new Map();
  constructor() {
    // Bind the prototype functions to this Console instance
    const keys = Object.getOwnPropertyNames(Console.prototype).filter(it => it !== 'constructor');
    for (const key of keys) {
      if (typeof Console.prototype[key] !== 'function') {
        break;
      }
      wrapConsole(this, vmConsole, key);
    }

    Object.defineProperty(this, Symbol.toStringTag, {
      configurable: true,
      enumerable: false,
      writable: false,
      value: 'console',
    });
  }

  assert(assertion, ...args) {
    if (!assertion) {
      this.#stderr.output(...args);
    }
  }

  log(...args) {
    this.#stdout.output(...args);
  }

  dir(...args) {
    this.#stdout.output(...args);
  }

  dirxml(...args) {
    this.#stdout.output(...args);
  }

  debug(...args) {
    this.#stdout.output(...args);
  }

  info(...args) {
    this.#stdout.output(...args);
  }

  warn(...args) {
    this.#stderr.output(...args);
  }

  error(...args) {
    this.#stderr.output(...args);
  }

  time(label) {
    if (this.#timeLabel.has(label)) {
      throw new Error(`Label '${label}' already exists for console.time()`);
    }
    this.#timeLabel.set(label, Date.now());
  }

  timeEnd(label) {
    const now = Date.now();
    if (!this.#timeLabel.has(label)) {
      throw new Error(`Label '${label}' does not exists for console.timeEnd()`);
    }
    const start = this.#timeLabel.get(label);
    this.#timeLabel.delete(label);
    this.warn(`${label}: ${now - start} ms`);
  }
}

// We have to bind the methods grabbed from the instance instead of from
// the prototype so that users extending the Console can override them
// from the prototype chain of the subclass.
function wrapConsole(console, vmConsole, key) {
  // Wrap a console implemented by Worker with features from the VM inspector.
  // Use native bind to create correct call sites that pointing to user codes.
  console[key] = consoleCall.bind(console, vmConsole[key], Console.prototype[key]);
  Object.defineProperty(console[key], 'name', {
    value: key,
  });
}


mod.Console = Console;
mod.customInspectSymbol = customInspectSymbol;
