'use strict';

const events = [];

let oninterval;
let interval;
let resolve;
const promise = new Promise(_resolve => {
  resolve = _resolve;
});

addEventListener('serialize', () => {
  events.push('serialize');
  interval = setInterval(() => {
    oninterval?.();
  }, 100);
});

addEventListener('deserialize', () => {
  events.push('deserialize');
  oninterval = () => {
    resolve();
    events.push('oninterval');
    clearInterval(interval);
  };
});

addEventListener('install', event => {
  events.push('install');
  event.waitUntil(promise.then(() => {
    events.push('installed');
    console.log(events.join(','));
  }));
});

events.push('main');
