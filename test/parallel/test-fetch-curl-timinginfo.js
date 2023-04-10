// META: flags=--experimental-curl-fetch
'use strict';

promise_test(async function() {
  const res = await fetch('http://localhost:30122/echo', { method: 'GET' });
  const timingInfo = res[Symbol.for('aworker:fetch_timing_info')];
  assert_equals(typeof timingInfo, 'object');
  assert_equals(typeof timingInfo.startTime, 'number');
  assert_equals(typeof timingInfo.totalTime, 'number');
  assert_equals(typeof timingInfo.namelookupTime, 'number');
  assert_equals(typeof timingInfo.connectTime, 'number');
  assert_equals(typeof timingInfo.appconnectTime, 'number');
  assert_equals(typeof timingInfo.pretransferTime, 'number');
  assert_equals(typeof timingInfo.starttransferTime, 'number');
  assert_equals(typeof timingInfo.redirectTime, 'number');
}, 'timinginfo property');

promise_test(async function() {
  const now = Date.now();
  const res = await fetch('http://localhost:30122/echo', { method: 'GET' });
  const timingInfo = res[Symbol.for('aworker:fetch_timing_info')];
  assert_true(timingInfo.startTime >= now);
  assert_true(Date.now() > timingInfo.startTime);
  assert_true(timingInfo.totalTime > timingInfo.starttransferTime);
  assert_true(timingInfo.starttransferTime > timingInfo.pretransferTime);
  // only valid for SSL connect
  assert_equals(timingInfo.appconnectTime, 0);
  assert_true(timingInfo.pretransferTime > timingInfo.connectTime);
  assert_true(timingInfo.connectTime >= timingInfo.namelookupTime);
  assert_true(timingInfo.namelookupTime > 0);

  // not implement
  assert_true(timingInfo.redirectTime >= 0);
}, 'property value');
