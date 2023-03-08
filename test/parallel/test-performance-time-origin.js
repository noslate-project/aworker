'use strict';

test(() => {
  assert_true(typeof performance.timeOrigin === 'number');
}, 'performance.timeOrigin');