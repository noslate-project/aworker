'use strict';

const path = require('path');
const common = require('../common/node_testharness');

promise_test(async () => {
  let err;

  try {
    await common.execAworker([ path.join(__dirname, 'fixtures/console-object.js'), '--max-old-space-size=10' ], {
      stdio: [ 'ignore', 'ignore', 'pipe' ],
    });
  } catch (e) {
    err = e;
  }

  // err exist when oom
  assert_true(err == null);
}, 'console object should not oom when inspector not active');