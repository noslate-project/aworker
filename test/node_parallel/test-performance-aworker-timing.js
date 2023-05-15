'use strict';

const path = require('path');
const common = require('../common/node_testharness');


promise_test(async () => {
  const result = await common.execAworker([
    '--mode=seed-userland',
    path.join(__dirname, './fixtures/performance.js'),
  ]);
  const timing = JSON.parse(result.stdout.trim()).aworkerTiming;

  const keys = [
    'bootstrapAgent',
    'loadEvent',
    'duration',
  ];

  for (let i = 0; i < keys.length; i++) {
    if (i === 0) {
      assert_greater_than(timing[keys[i]], 0);
      continue;
    }
    assert_greater_than_equal(timing[keys[i]], timing[keys[i - 1]]);
  }
}, 'milestones order in fork mode');

promise_test(async () => {
  const result = await common.execAworker([
    '-e',
    'setTimeout(()=>{console.log(JSON.stringify(performance.toJSON()));}, 0)',
  ]);
  const timing = JSON.parse(result.stdout.trim()).aworkerTiming;

  const keys = [
    'bootstrapAgent',
    'loadEvent',
    'duration',
  ];

  for (let i = 0; i < keys.length; i++) {
    if (i === 0) {
      assert_greater_than(timing[keys[i]], 0);
      continue;
    }
    assert_greater_than_equal(timing[keys[i]], timing[keys[i - 1]]);
  }
}, 'milestones order in REPL');
