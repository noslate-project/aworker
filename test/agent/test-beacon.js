'use strict';

test(() => {
  assert_true(aworker.sendBeacon('trace', { format: 'eagleeye' }, 'foobar'));
}, 'should send beacon');

test(() => {
  assert_throws_js(TypeError, () => aworker.sendBeacon(1, { format: 'eagleeye' }, 'foobar'));
  assert_throws_js(TypeError, () => aworker.sendBeacon('trace', { format: 1 }, 'foobar'));
  assert_throws_js(TypeError, () => aworker.sendBeacon('trace', { format: 'eagleeye' }, 1));
}, 'should validate args');
