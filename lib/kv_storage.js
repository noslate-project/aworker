'use strict';

const agentChannel = load('agent_channel');
const { TextDecoder, TextEncoder } = load('encoding');
const { DOMException } = load('dom/exception');
const { bufferLikeToUint8Array } = load('utils');

const kEmpty = new Uint8Array();
const kMaxKeyByteLength = 512;
const kDecoder = new TextDecoder();
const kEncoder = new TextEncoder();

function createError(status, message) {
  switch (status) {
    case 400: {
      return new DOMException(message ?? 'Quota exceeded limit.', 'QuotaExceededError');
    }
    case 404:
    case 409: {
      return new DOMException(message ?? 'Invalid state.', 'InvalidStateError');
    }
    default: {
      return new Error(message);
    }
  }
}

async function request(operation, metadata, value) {
  const { status, body } = await agentChannel.call('extensionBinding', {
    name: 'kv',
    operation,
    metadata: JSON.stringify(metadata),
    body: value,
  });
  if (status !== 200) {
    const message = kDecoder.decode(body);
    throw createError(status, message);
  }
  return body;
}

class KvStorage {
  #name;
  constructor(name) {
    this.#name = name;
  }

  async get(key) {
    if (typeof key !== 'string') {
      throw TypeError('Key must be a string');
    }
    const body = await request('get', {
      namespace: this.#name,
      key,
    }, kEmpty);
    return body;
  }

  async set(key, value) {
    if (typeof key !== 'string') {
      throw TypeError('Key must be a string');
    }
    const keyByteLength = kEncoder.encode(key).byteLength;
    if (keyByteLength > kMaxKeyByteLength) {
      throw TypeError('Key byte length must not exceed 512 bytes.');
    }
    value = bufferLikeToUint8Array(value);
    await request('set', {
      namespace: this.#name,
      key,
    }, value);
  }

  async delete(key) {
    if (typeof key !== 'string') {
      throw TypeError('Key must be a string');
    }
    await request('delete', {
      namespace: this.#name,
      key,
    }, kEmpty);
  }

  async list() {
    const body = await request('list', {
      namespace: this.#name,
    }, kEmpty);

    const result = JSON.parse(kDecoder.decode(body));
    return result;
  }
}

class KvStorages {
  async open(namespace, options) {
    if (typeof namespace !== 'string') {
      throw new TypeError('Namespace should be a string');
    }
    const lru = options?.evictionStrategy === 'lru';
    await request('open', {
      namespace,
      lru,
    }, kEmpty);
    return new KvStorage(namespace);
  }
}

wrapper.mod = {
  KvStorage,
  KvStorages,
  kvStorages: new KvStorages(),
};
