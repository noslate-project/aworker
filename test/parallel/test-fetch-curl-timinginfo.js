// META: flags=--experimental-curl-fetch
'use strict';

promise_test(async function() {
  const res = await fetch('http://localhost:30122/echo', { method: 'GET' });
  const timingInfo = res[Symbol.for('aworker:fetch_timing_info')];
  assert_true(timingInfo instanceof Object);
  assert_equals(typeof timingInfo.totalTime, 'number');
  assert_equals(typeof timingInfo.namelookupTime, 'number');
  assert_equals(typeof timingInfo.connectTime, 'number');
  assert_equals(typeof timingInfo.appconnectTime, 'number');
  assert_equals(typeof timingInfo.pretransferTime, 'number');
  assert_equals(typeof timingInfo.starttransferTime, 'number');
  assert_equals(typeof timingInfo.redirectTime, 'number');
}, 'timinginfo property');
