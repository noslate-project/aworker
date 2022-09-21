'use strict';

globalThis.fetched = false;
globalThis.cache = null;
addEventListener('serialize', event => {
  event.waitUntil((async () => {
    const resp = await fetch('http://localhost:30122/dump');
    const result = await resp.json();
    globalThis.cache = result;
    globalThis.fetched = true;
  })());
});

addEventListener('deserialize', () => {
  if (!globalThis.fetched) {
    throw new Error('test failed');
  }
  if (globalThis.cache == null) {
    throw new Error('test failed');
  }
  console.log('BUILD-SNAPSHOT: deserialized');
});
