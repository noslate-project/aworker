'use strict';

const { TextEncoder } = load('encoding');

const MAX_SIZE = 2 ** 32 - 2;
const CR = '\r'.charCodeAt(0);
const LF = '\n'.charCodeAt(0);

// `off` is the offset into `dst` where it will at which to begin writing values
// from `src`.
// Returns the number of bytes copied.
function copyBytes(src, dst, off = 0) {
  const r = dst.byteLength - off;
  if (src.byteLength > r) {
    src = src.subarray(0, r);
  }
  dst.set(src, off);
  return src.byteLength;
}

class GrowableBuffer {
  #uint8View = null; // contents are the bytes buf[off : len(buf)]
  #off = 0; // read at buf[off], write at buf[buf.byteLength]

  constructor(ab) {
    if (ab == null) {
      this.#uint8View = new Uint8Array(0);
      return;
    }

    this.#uint8View = new Uint8Array(ab);
  }

  bytes(options = { copy: true }) {
    if (options.copy === false) return this.#uint8View.subarray(this.#off);
    return this.#uint8View.slice(this.#off);
  }

  empty() {
    return this.#uint8View.byteLength <= this.#off;
  }

  get length() {
    return this.#uint8View.byteLength - this.#off;
  }

  get capacity() {
    return this.#uint8View.buffer.byteLength;
  }

  reset() {
    this.#reslice(0);
    this.#off = 0;
  }

  #tryGrowByReslice = n => {
    const l = this.#uint8View.byteLength;
    if (n <= this.capacity - l) {
      this.#reslice(l + n);
      return l;
    }
    return -1;
  };

  #reslice = len => {
    if (!(len <= this.#uint8View.buffer.byteLength)) {
      throw new Error('assert');
    }
    this.#uint8View = new Uint8Array(this.#uint8View.buffer, 0, len);
  };

  writeSync(p) {
    const m = this.#grow(p.byteLength);
    return copyBytes(p, this.#uint8View, m);
  }

  write(p) {
    const n = this.writeSync(p);
    return Promise.resolve(n);
  }

  #grow = num => {
    const len = this.length;
    // If buffer is empty, reset to recover space.
    if (len === 0 && this.#off !== 0) {
      this.reset();
    }
    // Fast: Try to grow by means of a reslice.
    const i = this.#tryGrowByReslice(num);
    if (i >= 0) {
      return i;
    }
    const cap = this.capacity;
    if (num <= Math.floor(cap / 2) - len) {
      // We can slide things down instead of allocating a new
      // ArrayBuffer. We only need m+n <= c to slide, but
      // we instead let capacity get twice as large so we
      // don't spend all our time copying.
      copyBytes(this.#uint8View.subarray(this.#off), this.#uint8View);
    } else if (cap + num > MAX_SIZE) {
      throw new Error('The buffer cannot be grown beyond the maximum size.');
    } else {
      // Not enough space anywhere, we need to allocate.
      const buf = new Uint8Array(Math.min(2 * cap + num, MAX_SIZE));
      copyBytes(this.#uint8View.subarray(this.#off), buf);
      this.#uint8View = buf;
    }
    // Restore this.#off and len(this.#buf).
    this.#off = 0;
    this.#reslice(Math.min(len + num, MAX_SIZE));
    return len;
  };

  grow(n) {
    if (n < 0) {
      throw Error('Buffer.grow: negative count');
    }
    const m = this.#grow(n);
    this.#reslice(m);
  }
}

async function bufferFromStream(stream, size) {
  const buffer = new GrowableBuffer();

  if (size) {
    // grow to avoid unnecessary allocations & copies
    buffer.grow(size);
  }

  const encoder = new TextEncoder();
  while (true) {
    const { done, value } = await stream.read();
    if (done) {
      break;
    }
    if (typeof value === 'string') {
      buffer.writeSync(encoder.encode(value));
    } else if (value instanceof ArrayBuffer) {
      buffer.writeSync(new Uint8Array(value));
    } else if (value instanceof Uint8Array) {
      buffer.writeSync(value);
    } else if (!value) {
      // noop for undefined
    } else {
      throw new Error('unhandled type on stream read');
    }
  }

  return buffer.bytes().buffer;
}

function getHeaderValueParams(value) {
  const params = new Map();
  // Forced to do so for some Map constructor param mismatch
  value
    .split(';')
    .slice(1)
    .map(s => s.trim().split('='))
    .filter(arr => arr.length > 1)
    .map(([ k, v ]) => [ k, v.replace(/^"([^"]*)"$/, '$1') ])
    .forEach(([ k, v ]) => params.set(k, v));
  return params;
}

function hasHeaderValueOf(s, value) {
  return new RegExp(`^${value}[\t\s]*;?`).test(s);
}

wrapper.mod = {
  CR,
  LF,
  GrowableBuffer,
  bufferFromStream,
  getHeaderValueParams,
  hasHeaderValueOf,
};
