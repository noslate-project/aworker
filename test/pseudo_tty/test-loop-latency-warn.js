// META: flags=--long-task-threshold-ms=15
'use strict';

function test() {
  const start = Date.now();
  while (Date.now() - start <= 35) {
    /** busy loop */
  }
}

function main() {
  test();
  setTimeout(() => {
    test();
  }, 1000);
}

main();
