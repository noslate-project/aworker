'use strict';

globalThis.snapshotObject = {
  foo: 'bar',
};

console.log('not executed in deserialization');
const events = [];
addEventListener('deserialize', event => {
  events.push('deserialized');
  event.waitUntil(Promise.resolve().then(async () => {
    events.push('deserialization finished');
  }));
});

addEventListener('install', event => {
  events.push('install');
  event.waitUntil(Promise.resolve().then(async () => {
    events.push('installation finished');
    console.log('BUILD-SNAPSHOT:', events.join(','));
  }));
});
