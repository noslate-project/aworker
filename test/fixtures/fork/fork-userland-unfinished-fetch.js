'use strict';

const events = [];

let promise;

addEventListener('serialize', () => {
  events.push('serialize');
  promise = fetch('http://not-exist-host')
    .then(
      () => 'unexpected-resolution',
      err => {
        // TODO(chengzhong.wcz): normalize error codes.
        if (err.message.includes('CanonicalCode::CONNECTION_RESET') || err.message.includes('CURLE_COULDNT_RESOLVE_HOST')) {
          return 'expected-rejection';
        }
        return 'unexpected-rejection';
      }
    );
});

addEventListener('deserialize', event => {
  events.push('deserialize');
  event.waitUntil(promise.then(value => {
    events.push(value);
  }));
});

addEventListener('install', event => {
  events.push('install');
  event.waitUntil(promise.then(() => {
    events.push('installed');
    console.log(events.join(','));
  }));
});

events.push('main');
