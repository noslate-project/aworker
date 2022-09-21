'use strict';

const events = [];

addEventListener('serialize', event => {
  events.push('serialize');
  event.waitUntil(Promise.resolve().then(() => {
    events.push('serialized');
  }));
});

addEventListener('deserialize', event => {
  events.push('deserialize');
  event.waitUntil(Promise.resolve().then(() => {
    events.push('deserialized');
  }));
});

addEventListener('install', event => {
  events.push('install');
  event.waitUntil(Promise.resolve().then(() => {
    events.push('installed');
    console.log(events.join(','));
  }));
});

events.push('main');
