'use strict';

function getAworkerTiming() {
  return { ...performance.aworkerTiming };
}

test(() => {
  const timing = getAworkerTiming();
  assert_array_equals(Object.keys(timing), [
    'name',
    'entryType',
    'startTime',
    'duration',
    'bootstrapAgent',
    'loadEvent',
    'idleTime',
  ]);
  assert_equals(timing.name, 'aworker');
  assert_equals(timing.entryType, 'aworker');
}, 'aworker milestones');

promise_test(async () => {
  const timing = await new Promise(resolve => {
    setTimeout(() => {
      resolve(getAworkerTiming());
    }, 0);
  });
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
}, 'milestones order');
