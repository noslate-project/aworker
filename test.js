if (!globalThis.aworker) {
  fetch = require('../../../undici').fetch;
}
async function main() {

  const obs = new PerformanceObserver((list) => {
    {
      const entries = list.getEntries();
      console.log(entries.length);
      entries.forEach(v => {
        console.log(v);
      })
    }
    // {
    //   const entries = list.getEntriesByType('resource');
    //   console.log(entries.length);
    // }
    // {
    //   const entries = list.getEntriesByName('http://localhost:12345');
    //   console.log(entries.length);
    // }
    // obs.disconnect();
  });
  obs.observe({ entryTypes: ['resource'] });

  const timingInfo = createTimingInfo({ finalConnectionTimingInfo: {} });
  const customGlobal = {};
  const requestedUrl = 'http://localhost:12345';
  const cacheMode = 'local';
  const initiatorType = 'fetch';

  const fetchPromises = [];
  for (let i = 0; i < 50; i++) {
    fetchPromises.push((fetch('http://localhost:12345').then(r => r.text())));
    // const resource = performance.markResourceTiming(
    //   timingInfo,
    //   requestedUrl,
    //   initiatorType,
    //   customGlobal,
    //   cacheMode,
    // );
  }
  const r = await Promise.all(fetchPromises);
  // console.log(r);

  // performance.clearResourceTimings();
  // const entries = performance.getEntries();
}
main();


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
  finalConnectionTimingInfo = null
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