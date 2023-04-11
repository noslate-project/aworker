'use strict';

const { eventLoopUtilization } = load('performance/utils');

test(() => {
  const u1 = eventLoopUtilization();
  assert_array_equals(Object.keys(u1), [
    'idle',
    'active',
    'utilization',
  ]);
}, 'should generate elu');

test(() => {
  const u1 = eventLoopUtilization();
  for (let i = 0; i < 1000_000; i++) { /** empty */ }
  const u2 = eventLoopUtilization();
  const u3 = eventLoopUtilization(u2, u1);
  assert_approx_equals(u3.idle, 0, 100);
  assert_true(u3.active > 0);
  assert_equals(u3.utilization, 1);

  const u4 = eventLoopUtilization(u1);
  assert_approx_equals(u4.idle, 0, 100);
  assert_true(u4.active > 0);
  assert_equals(u4.utilization, 1);
}, 'should compute diff');

promise_test(async () => {
  const u1 = eventLoopUtilization();

  await new Promise(resolve => setTimeout(resolve, 1000));

  const u2 = eventLoopUtilization();
  const u3 = eventLoopUtilization(u2, u1);
  assert_approx_equals(u3.idle, 1000, 100);
  assert_approx_equals(u3.active, 1, 100);
  assert_true(u3.utilization > 0);
  assert_true(u3.utilization < 1);
}, 'should get idle times');
