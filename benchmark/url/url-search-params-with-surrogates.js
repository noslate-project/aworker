'use strict';

createBenchmark(main, {
  n: [ 1e5 ],
  size: [ 10, 100, 500 ],
});

function main({ n, size }, bench) {
  const key = 'string\ud801';
  const value = key.repeat(size);

  bench.start();
  for (let i = 0; i < n; i++) {
    new URLSearchParams({ [key]: value });
  }
  bench.end(n);
}
