'use strict';

const exitCode = Number.parseInt(aworker.env.EXIT_CODE ?? '1');

setTimeout(() => {
  console.log('this line should not be printed');
});
aworker.exit(exitCode);
console.log('this line should not be printed');
