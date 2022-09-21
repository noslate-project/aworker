'use strict';

const path = require('path');
const common = require('../common/node_testharness');

promise_test(async () => {
  let err;
  try {
    await common.execAworker([ path.join(__dirname, 'fixtures/syntax-error.js') ]);
  } catch (e) {
    err = e;
  }
  assert_true(err != null);
  assert_equals(err.code, 1);

  const lines = err.stderr.split('\n');
  assert_regexp_match(lines[0], /^\/.*\/fixtures\/syntax-error.js:1$/);
  assert_regexp_match(err.stderr, /SyntaxError/);
}, 'exit syntax error printed');
