'use strict';

createBenchmark(main, {
  n: [ 1e6 ],
});

async function run(headers, n) {
  for (let i = 0; i < n; ++i) {
    new Headers(headers);
  }
}

function main({ n }, bench) {
  bench.start();
  const headers = new Headers([
    [ 'server', 'MyServer' ],
    [ 'content-type', 'text/plain' ],
    [ 'content-length', '132210' ],
    [ 'date', 'Mon, 05 Jun 2023 09:31:30 GMT' ],
    [ 'vary', 'Accept-Encoding' ],
    [ 'x-oss-request-id', '647DAB72A68BAC3030E97FB7' ],
    [ 'x-oss-cdn-auth', 'success' ],
    [ 'accept-ranges', 'bytes' ],
    [ 'etag', '"A659284B30143F1498153E4885F88558"' ],
    [ 'last-modified', 'Thu, 16 Mar 2023 07:47:18 GMT' ],
    [ 'x-oss-object-type', 'Normal' ],
    [ 'x-oss-storage-class', 'Standard' ],
    [ 'x-oss-meta-file-type', 'css' ],
    [ 'x-oss-meta-filename', 'foobar.txt' ],
    [ 'access-control-allow-origin', '*' ],
    [ 'cache-control', 'max-age=2592000' ],
    [ 'content-md5', 'plkoSzAUPxSYFT5IhfiFWA==' ],
    [ 'x-oss-server-time', '4' ],
    [ 'via', 'my-proxy' ],
    [ 'x-oss-hash-crc64ecma', '4246626377657596981' ],
    [ 'age', '596440' ],
    [ 'x-cache', 'HIT TCP_MEM_HIT dirn:10:77483806' ],
    [ 'x-swift-savetime', 'Fri, 09 Jun 2023 13:26:23 GMT' ],
    [ 'x-swift-cachetime', '2232307' ],
    [ 'timing-allow-origin', '*' ],
    [ 'x-trace-id', '2f76e34316865539308022840e' ],
  ]);
  run(headers, n);
  bench.end(n);
}
