'use strict';

function foo() {
  for (let i = 0; i < 100000; i++) {
    new URL('file:///');
  }
}

function main() {
  foo();
}

main();
