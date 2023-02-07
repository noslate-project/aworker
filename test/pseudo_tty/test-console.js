'use strict';
function foo() {
  console.trace('I', 'am', 'here');
}

function bar() {
  foo();
}

bar();
