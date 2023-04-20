'use strict';

function myInnerFunction(...args) {
  if (args.join(',') !== '1,2,3,4') {
    throw new Error('invalid arguments');
  }
}

(function myOuterFunction() {
  const receiver = { brand: 'my-receiver' };
  aworker.inspector.callAndPauseOnFirstStatement(myInnerFunction, receiver, 1, 2, 3, 4);
})();
