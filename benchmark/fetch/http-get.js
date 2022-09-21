// META: flags=--experimental-curl-fetch
// META: resource-server=true
'use strict';

createBenchmark(main, {
  n: [ 1e5 ],
  parallel: [ 5, 50 ],
});

function request() {
  return fetch('http://localhost:30122/ping')
    .then(res => {
      return res.text();
    });
}

async function run(n) {
  for (let i = 0; i < n; ++i) {
    await request();
  }
}

function main({ n, parallel }, bench) {
  bench.start();
  const nPerRun = Math.floor(n / parallel);
  Promise.all(new Array(parallel).fill(0).map(() => run(nPerRun)))
    .then(() => {
      bench.end(n);
    });
}
