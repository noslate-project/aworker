'use strict';

const path = require('path');
const common = require('../common/node_testharness');

promise_test(async () => {
  let err;
  try {
    await common.execAworker([ path.join(__dirname, 'fixtures/exit-code.js') ], {
      env: {
        EXIT_CODE: 2,
      },
    });
  } catch (e) {
    err = e;
  }
  assert_true(err != null);
  assert_equals(err.code, 2);
  assert_regexp_match(err.stdout, /^\s*$/);
}, 'exit with non-zero code');

promise_test(async () => {
  const { stdout } = await common.execAworker([ path.join(__dirname, 'fixtures/exit-code.js') ], {
    env: {
      EXIT_CODE: 0,
    },
  });
  assert_regexp_match(stdout, /^\s*$/);
}, 'exit with code 0');
