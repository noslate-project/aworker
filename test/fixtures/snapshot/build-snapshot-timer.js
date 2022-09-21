'use strict';

globalThis.foobar = 'quz';
let events = [];
addEventListener('serialize', event => {
  event.waitUntil(new Promise(resolve => {
    setTimeout(() => {
      globalThis.foobar = 'foobar';
      events.push('timer');
      resolve();
    }, 100);
  }));
});

addEventListener('deserialize', () => {
  // clear the event.
  events = [];
  if (globalThis.foobar !== 'foobar') {
    throw new Error('test failed');
  }
  setTimeout(() => {
    events.push('deserialized');
    console.log('BUILD-SNAPSHOT:', events.join(','));
  }, 200);
});
