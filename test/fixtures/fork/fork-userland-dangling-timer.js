'use strict';

const events = [];

const timer = setTimeout(() => {
  throw new Error('unreachable');
}, 10_000);

addEventListener('serialize', () => {
  events.push('serialize');
});

addEventListener('deserialize', () => {
  events.push('deserialize');
  clearTimeout(timer);
});

addEventListener('install', () => {
  events.push('install');
  console.log(events.join(','));
});

events.push('main');
