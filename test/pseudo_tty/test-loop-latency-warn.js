// META: flags=--long-task-threshold-ms=15
'use strict';

function test() {
  const start = Date.now();
  while (Date.now() - start <= 35) {
    /** busy loop */
  }
}

function main() {
  // Schedule two task in one loop tick.
  setTimeout(function timer1() {
    test();
  }, 1);
  setTimeout(function timer2() {
    test();
  }, 1);

  // busy loop.
  test();
}

main();
