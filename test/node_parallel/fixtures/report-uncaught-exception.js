'use strict';

function main() {
  const err = new Error('foobar');
  err.foo = 'bar';
  throw err;
}

main();
