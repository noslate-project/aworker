'use strict';

const events = [];

addEventListener('install', event => {
  events.push('install');

  setTimeout(() => {
    // never reach
  }, 50000);

  event.waitUntil(Promise.resolve().then(() => {
    events.push('installed');
  }));
});

addEventListener('uninstall', event => {
  events.push('uninstall');
  event.waitUntil(Promise.resolve().then(() => {
    events.push('uninstalled');
    console.log(events.join(','));
  }));
});
