'use strict';

addEventListener('error', () => {
  throw 'fatal';
});

throw new Error('foobar');
