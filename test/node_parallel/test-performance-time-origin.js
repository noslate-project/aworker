'use strict';

const path = require('path');
const common = require('../common/node_testharness');

promise_test(async () => {
  const instance_1 = await common.execAworker([ path.join(__dirname, 'fixtures', 'performance-time-origin.js') ]).catch(e => e);
  const instance_2 = await common.execAworker([ path.join(__dirname, 'fixtures', 'performance-time-origin.js') ]).catch(e => e);

  assert_true(!isNaN(parseFloat(instance_1.stdout)));
  assert_true(!isNaN(parseFloat(instance_2.stdout)));

  assert_true(instance_1.stdout !== instance_2.stdout);
}, 'performance.timeOrigin between aworker instance');
