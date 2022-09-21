'use strict';

globalThis.foobar = 'quz';
let events = [];
const p = Promise.resolve()
  .then(() => {
    return new Promise(resolve => {
      queueMicrotask(() => {
        events.push('microtask');
        globalThis.foobar = 'foobar';
        resolve('foobar');
      });
    });
  });

addEventListener('serialize', event => {
  event.waitUntil(p);
});

addEventListener('deserialize', event => {
  // clear the event.
  events = [];
  if (globalThis.foobar !== 'foobar') {
    throw new Error('test failed');
  }
  event.waitUntil(p.then(val => {
    events.push('deserialized');
    if (val !== 'foobar') {
      throw new Error('test failed');
    }
    console.log('BUILD-SNAPSHOT:', events.join(','));
  }));
});
