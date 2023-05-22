'use strict';

const path = require('path');
const common = require('../common/node_testharness');

promise_test(async () => {
  // should invoke success
  // throw error when oom or other exceptions
  await common.execAworker([ path.join(__dirname, 'fixtures/console-object.js'), '--max-old-space-size=10' ], {
    stdio: [ 'ignore', 'ignore', 'pipe' ],
  });

}, 'console object should not oom when inspector not active');
