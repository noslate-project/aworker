'use strict';
const { PerformanceEntry, InternalPerformanceEntry } = load('performance/performance_entry');
const { enqueue, bufferResourceTiming } = load('performance/observer');

const kInitiatorType = Symbol('kInitiatorType');
const kRequestedUrl = Symbol('kRequestedUrl');

const kTimingInfo = Symbol('kTimingInfo');
const kCacheMode = Symbol('kCacheMode');

class PerformanceResourceTiming extends PerformanceEntry {
  constructor() {
    throw new TypeError('illegal constructor');
  }
  get name() { return this[kRequestedUrl]; }
  get startTime() { return this[kTimingInfo].startTime; }
  get duration() { return this[kTimingInfo].endTime - this[kTimingInfo].startTime; }

  get initiatorType() { return this[kInitiatorType]; }
  get workerStart() { return this[kTimingInfo].finalServiceWorkerStartTime; }
  get redirectStart() { return this[kTimingInfo].redirectStartTime; }
  get redirectEnd() { return this[kTimingInfo].redirectEndTime; }
  get fetchStart() { return this[kTimingInfo].postRedirectStartTime; }
  get domainLookupStart() { return this[kTimingInfo].finalConnectionTimingInfo?.domainLookupStartTime; }
  get domainLookupEnd() { return this[kTimingInfo].finalConnectionTimingInfo?.domainLookupEndTime; }
  get connectStart() { return this[kTimingInfo].finalConnectionTimingInfo?.connectionStartTime; }
  get connectEnd() { return this[kTimingInfo].finalConnectionTimingInfo?.connectionEndTime; }
  get secureConnectionStart() { return this[kTimingInfo].finalConnectionTimingInfo?.secureConnectionStartTime; }
  get nextHopProtocol() { return this[kTimingInfo].finalConnectionTimingInfo?.ALPNNegotiatedProtocol; }
  get requestStart() { return this[kTimingInfo].finalNetworkRequestStartTime; }
  get responseStart() { return this[kTimingInfo].finalNetworkResponseStartTime; }
  get responseEnd() { return this[kTimingInfo].endTime; }
  get encodedBodySize() { return this[kTimingInfo].encodedBodySize; }
  get decodedBodySize() { return this[kTimingInfo].decodedBodySize; }
  get transferSize() {
    if (this[kCacheMode] === 'local') return 0;
    if (this[kCacheMode] === 'validated') return 300;

    return this[kTimingInfo].encodedBodySize + 300;
  }

  toJSON() {
    return {
      name: this.name,
      entryType: this.entryType,
      startTime: this.startTime,
      duration: this.duration,
      initiatorType: this[kInitiatorType],
      nextHopProtocol: this.nextHopProtocol,
      workerStart: this.workerStart,
      redirectStart: this.redirectStart,
      redirectEnd: this.redirectEnd,
      fetchStart: this.fetchStart,
      domainLookupStart: this.domainLookupStart,
      domainLookupEnd: this.domainLookupEnd,
      connectStart: this.connectStart,
      connectEnd: this.connectEnd,
      secureConnectionStart: this.secureConnectionStart,
      requestStart: this.requestStart,
      responseStart: this.responseStart,
      responseEnd: this.responseEnd,
      transferSize: this.transferSize,
      encodedBodySize: this.encodedBodySize,
      decodedBodySize: this.decodedBodySize,
    };
  }

  get [Symbol.toStringTag]() {
    return 'PerformanceResourceTiming';
  }
}

function InternalPerformanceResourceTiming(requestedUrl, initiatorType, timingInfo, cacheMode = '') {
  InternalPerformanceEntry.call(this, requestedUrl, 'resource');
  this[kInitiatorType] = initiatorType;
  this[kRequestedUrl] = requestedUrl;
  this[kTimingInfo] = timingInfo;
  this[kCacheMode] = cacheMode;
  Object.setPrototypeOf(this, PerformanceResourceTiming.prototype);
}

function markResourceTiming(timingInfo, requestedUrl, initiatorType, global, cacheMode) {
  const resource = new InternalPerformanceResourceTiming(requestedUrl, initiatorType, timingInfo, cacheMode);
  enqueue(resource);
  bufferResourceTiming(resource);
  return resource;
}

wrapper.mod = {
  markResourceTiming,
  PerformanceResourceTiming,
};
