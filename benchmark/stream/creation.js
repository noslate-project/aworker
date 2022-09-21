'use strict';

createBenchmark(main, {
  n: [ 5e4 ],
  kind: [ 'readable', 'writable' ],
});

function main({ n, kind }, bench) {
  switch (kind) {
    case 'readable':
      bench.start();
      for (let i = 0; i < n; ++i) {
        // TODO: memory leaks on stream
        new ReadableStream();
      }
      bench.end(n);
      break;
    case 'writable':
      bench.start();
      for (let i = 0; i < n; ++i) {
        new WritableStream();
      }
      bench.end(n);
      break;
    default:
      throw new Error('Invalid kind');
  }
}
