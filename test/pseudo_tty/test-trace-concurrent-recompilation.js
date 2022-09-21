// META: flags=--trace-concurrent-recompilation
/* eslint-disable */

/**
 * No concurrent-recompilation should be observed in trace for following
 * function.
 */
function main(it) {
  for (let x = 0; x < 10000; ++x) {
    [(x) => x, [, 4294967295].find((x) => x), , 2].includes(it);
  }
}

(function() {
  for (let i = 0; i < 1000; ++i) {
    main(i);
  }
})();

console.log('done')
