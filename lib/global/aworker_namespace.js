'use strict';

const process = loadBinding('process');
const {
  defineGlobalProperties,
  defineGlobalLazyProperties,
} = load('global/utils');

function AworkerNamespace() {
  throw new TypeError('Illegal constructor');
}

function InternalAworkerNamespace() {
  if (new.target !== InternalAworkerNamespace) {
    return new InternalAworkerNamespace();
  }

  Object.setPrototypeOf(this, AworkerNamespace.prototype);

  defineGlobalProperties(this, {
    arch: process.arch,
    platform: process.platform,
    env: process.env,
    version: process.version,
  });

  defineGlobalLazyProperties(this, {
    AsyncLocalStorage: { path: 'task_queue', property: 'AsyncLocalStorage' },
    crypto: { path: 'crypto' },
    Dapr: { path: 'dapr', property: 'Dapr' },
    idna: { path: 'idna' },
    js: { path: 'js', property: '__exposed__' },
    zlib: { path: 'zlib' },
    exit: { path: 'process/methods', property: 'publicExit' },
    kvStorages: { path: 'kv_storage', property: 'kvStorages' },
    sendBeacon: { path: 'beacon', property: 'sendBeacon' },
  });

  return this;
}

wrapper.mod = {
  AworkerNamespace,
  InternalAworkerNamespace,
};
