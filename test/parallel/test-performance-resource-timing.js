'use strict';
// META: flags=--expose-internals
const { formatWithOptions } = load('console/format');
const { PerformanceEntry } = load('performance/performance_entry');
const {
  markResourceTiming,
  PerformanceResourceTiming,
} = load('performance/resource_timing');

test(function() {
  assert_throws_js(TypeError, () => new PerformanceResourceTiming());
}, 'throw with new');

test(function() {
  const timingInfo = createTimingInfo({ finalConnectionTimingInfo: {} });
  const requestedUrl = 'http://localhost:8080';
  const cacheMode = 'local';
  const initiatorType = 'fetch';
  const resource = markResourceTiming(
    timingInfo,
    requestedUrl,
    initiatorType,
    cacheMode
  );

  assert_true(resource instanceof PerformanceEntry);
  assert_true(resource instanceof PerformanceResourceTiming);
  {
    const entries = performance.getEntries();
    assert_equals(entries.length, 1);
    assert_true(entries[0] instanceof PerformanceResourceTiming);
  }

  {
    const entries = performance.getEntriesByType('resource');
    assert_equals(entries.length, 1);
    assert_true(entries[0] instanceof PerformanceResourceTiming);
  }

  {
    const entries = performance.getEntriesByName(resource.name);
    assert_equals(entries.length, 1);
    assert_true(entries[0] instanceof PerformanceResourceTiming);
  }

  performance.clearResourceTimings();
  assert_equals(performance.getEntries().length, 0);
}, 'performance.getEntries');


// Assert resource data based in timingInfo

// default values
test(function() {
  const timingInfo = createTimingInfo({ finalConnectionTimingInfo: {} });
  const requestedUrl = 'http://localhost:8080';
  const cacheMode = 'local';
  const initiatorType = 'fetch';
  const resource = markResourceTiming(
    timingInfo,
    requestedUrl,
    initiatorType,
    cacheMode
  );

  assert_true(resource instanceof PerformanceEntry);
  assert_true(resource instanceof PerformanceResourceTiming);

  assert_equals(resource.entryType, 'resource');
  assert_equals(resource.name, requestedUrl);
  assert_true(typeof resource.cacheMode === 'undefined');
  assert_equals(resource.startTime, timingInfo.startTime);
  assert_equals(resource.duration, 0);
  assert_equals(resource.initiatorType, initiatorType);
  assert_equals(resource.workerStart, 0);
  assert_equals(resource.redirectStart, 0);
  assert_equals(resource.redirectEnd, 0);
  assert_equals(resource.fetchStart, 0);
  assert_equals(resource.domainLookupStart, 0);
  assert_equals(resource.domainLookupEnd, 0);
  assert_equals(resource.connectStart, 0);
  assert_equals(resource.connectEnd, 0);
  assert_equals(resource.secureConnectionStart, 0);
  assert_array_equals(resource.nextHopProtocol, []);
  assert_equals(resource.requestStart, 0);
  assert_equals(resource.responseStart, 0);
  assert_equals(resource.responseEnd, 0);
  assert_equals(resource.encodedBodySize, 0);
  assert_equals(resource.decodedBodySize, 0);
  assert_equals(resource.transferSize, 0);
  assert_object_equals(resource.toJSON(), {
    name: requestedUrl,
    entryType: 'resource',
    startTime: 0,
    duration: 0,
    initiatorType,
    nextHopProtocol: [],
    workerStart: 0,
    redirectStart: 0,
    redirectEnd: 0,
    fetchStart: 0,
    domainLookupStart: 0,
    domainLookupEnd: 0,
    connectStart: 0,
    connectEnd: 0,
    secureConnectionStart: 0,
    requestStart: 0,
    responseStart: 0,
    responseEnd: 0,
    transferSize: 0,
    encodedBodySize: 0,
    decodedBodySize: 0,
  });
  assert_equals(formatWithOptions({}, performance.getEntries()), `[ PerformanceResourceTiming { name: 'http://localhost:8080',
    entryType: 'resource',
    startTime: 0,
    duration: 0,
    initiatorType: 'fetch',
    nextHopProtocol: [],
    workerStart: 0,
    redirectStart: 0,
    redirectEnd: 0,
    fetchStart: 0,
    domainLookupStart: 0,
    domainLookupEnd: 0,
    connectStart: 0,
    connectEnd: 0,
    secureConnectionStart: 0,
    requestStart: 0,
    responseStart: 0,
    responseEnd: 0,
    transferSize: 0,
    encodedBodySize: 0,
    decodedBodySize: 0 } ]`);

  assert_equals(formatWithOptions({}, resource), `PerformanceResourceTiming { name: 'http://localhost:8080',
  entryType: 'resource',
  startTime: 0,
  duration: 0,
  initiatorType: 'fetch',
  nextHopProtocol: [],
  workerStart: 0,
  redirectStart: 0,
  redirectEnd: 0,
  fetchStart: 0,
  domainLookupStart: 0,
  domainLookupEnd: 0,
  connectStart: 0,
  connectEnd: 0,
  secureConnectionStart: 0,
  requestStart: 0,
  responseStart: 0,
  responseEnd: 0,
  transferSize: 0,
  encodedBodySize: 0,
  decodedBodySize: 0 }`);

  assert_true(resource instanceof PerformanceEntry);
  assert_true(resource instanceof PerformanceResourceTiming);

  performance.clearResourceTimings();
  const entries = performance.getEntries();
  assert_equals(entries.length, 0);
}, 'performance values');


test(function() {
  const timingInfo = createTimingInfo({
    endTime: 100,
    startTime: 50,
    encodedBodySize: 150,
  });
  const requestedUrl = 'http://localhost:8080';
  const cacheMode = '';
  const initiatorType = 'fetch';
  const resource = markResourceTiming(
    timingInfo,
    requestedUrl,
    initiatorType,
    cacheMode
  );

  assert_true(resource instanceof PerformanceEntry);
  assert_true(resource instanceof PerformanceResourceTiming);

  assert_equals(resource.entryType, 'resource');
  assert_equals(resource.name, requestedUrl);
  assert_true(typeof resource.cacheMode === 'undefined');
  assert_equals(resource.startTime, timingInfo.startTime);
  // Duration should be the timingInfo endTime - startTime
  assert_equals(resource.duration, 50);
  // TransferSize should be encodedBodySize + 300 when cacheMode is empty
  assert_equals(resource.transferSize, 450);

  assert_true(resource instanceof PerformanceEntry);
  assert_true(resource instanceof PerformanceResourceTiming);

  performance.clearResourceTimings();
  const entries = performance.getEntries();
  assert_equals(entries.length, 0);
}, 'custom  getters math');

test(function() {
  const obs = new PerformanceObserver(list => {
    {
      const entries = list.getEntries();
      assert_equals(entries.length, 1);
      assert_true(entries[0] instanceof PerformanceResourceTiming);
    }
    {
      const entries = list.getEntriesByType('resource');
      assert_equals(entries.length, 1);
      assert_true(entries[0] instanceof PerformanceResourceTiming);
    }
    {
      const entries = list.getEntriesByName('http://localhost:8080');
      assert_equals(entries.length, 1);
      assert_true(entries[0] instanceof PerformanceResourceTiming);
    }
    obs.disconnect();
  });
  obs.observe({ entryTypes: [ 'resource' ] });

  const timingInfo = createTimingInfo({ finalConnectionTimingInfo: {} });
  const requestedUrl = 'http://localhost:8080';
  const cacheMode = 'local';
  const initiatorType = 'fetch';
  const resource = markResourceTiming(
    timingInfo,
    requestedUrl,
    initiatorType,
    cacheMode
  );

  assert_true(resource instanceof PerformanceEntry);
  assert_true(resource instanceof PerformanceResourceTiming);

  performance.clearResourceTimings();
  const entries = performance.getEntries();
  assert_equals(entries.length, 0);
}, 'Using PerformanceObserver');

function createTimingInfo({
  startTime = 0,
  redirectStartTime = 0,
  redirectEndTime = 0,
  postRedirectStartTime = 0,
  finalServiceWorkerStartTime = 0,
  finalNetworkRequestStartTime = 0,
  finalNetworkResponseStartTime = 0,
  endTime = 0,
  encodedBodySize = 0,
  decodedBodySize = 0,
  finalConnectionTimingInfo = null,
}) {
  if (finalConnectionTimingInfo !== null) {
    finalConnectionTimingInfo.domainLookupStartTime =
      finalConnectionTimingInfo.domainLookupStartTime || 0;
    finalConnectionTimingInfo.domainLookupEndTime =
      finalConnectionTimingInfo.domainLookupEndTime || 0;
    finalConnectionTimingInfo.connectionStartTime =
      finalConnectionTimingInfo.connectionStartTime || 0;
    finalConnectionTimingInfo.connectionEndTime =
      finalConnectionTimingInfo.connectionEndTime || 0;
    finalConnectionTimingInfo.secureConnectionStartTime =
      finalConnectionTimingInfo.secureConnectionStartTime || 0;
    finalConnectionTimingInfo.ALPNNegotiatedProtocol =
      finalConnectionTimingInfo.ALPNNegotiatedProtocol || [];
  }
  return {
    startTime,
    redirectStartTime,
    redirectEndTime,
    postRedirectStartTime,
    finalServiceWorkerStartTime,
    finalNetworkRequestStartTime,
    finalNetworkResponseStartTime,
    endTime,
    encodedBodySize,
    decodedBodySize,
    finalConnectionTimingInfo,
  };
}
