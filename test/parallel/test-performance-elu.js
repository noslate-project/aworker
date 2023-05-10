'use strict';

const { eventLoopUtilization } = performance;

test(() => {
  const u1 = eventLoopUtilization();
  assert_array_equals(Object.keys(u1), [
    'idle',
    'active',
    'utilization',
  ]);
}, 'should generate elu');
