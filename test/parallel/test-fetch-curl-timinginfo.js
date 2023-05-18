// META: flags=--experimental-curl-fetch
'use strict';

promise_test(async function() {
  const res = await fetch('http://localhost:30122/echo', { method: 'GET' });
  const timingInfo = res[Symbol.for('aworker:fetch_timing_info')];
  assert_equals(typeof timingInfo, 'object');

  const keys = [
    'startTime',
    'totalTime',
    'namelookupTime',
    'connectTime',
    'appconnectTime',
    'pretransferTime',
    'starttransferTime',
    'redirectTime',
    'eventLoopIdle',
    'eventLoopActive',
    'eventLoopUtilization',
  ];
  for (const key of keys) {
    assert_equals(typeof timingInfo[key], 'number', key);
  }
}, 'timinginfo property');

promise_test(async function() {
  const now = Date.now();
  const res = await fetch('http://localhost:30122/echo', { method: 'GET' });
  const timingInfo = res[Symbol.for('aworker:fetch_timing_info')];
  assert_true(timingInfo.startTime >= now);
  assert_true(Date.now() >= timingInfo.startTime);
  assert_true(timingInfo.totalTime > timingInfo.starttransferTime);
  assert_true(timingInfo.starttransferTime > timingInfo.pretransferTime);
  // only valid for SSL connect
  assert_equals(timingInfo.appconnectTime, 0);
  assert_true(timingInfo.pretransferTime > timingInfo.connectTime);
  assert_true(timingInfo.connectTime >= timingInfo.namelookupTime);
  assert_true(timingInfo.namelookupTime > 0);

  // event loop utilization
  assert_true(timingInfo.eventLoopIdle > 0);
  assert_true(timingInfo.eventLoopActive > 0);
  assert_true(timingInfo.eventLoopUtilization < 1);

  // not implement
  assert_true(timingInfo.redirectTime >= 0);
}, 'property value');
