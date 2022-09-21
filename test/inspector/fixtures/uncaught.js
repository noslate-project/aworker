'use strict';

const foo = 'bar';
(function foobar() {
  throw foo;
})();
