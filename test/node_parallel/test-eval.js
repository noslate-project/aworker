'use strict';

const common = require('../common/node_testharness');

promise_test(async () => {
  const { stdout } = await common.execAworker([ '--eval', 'console.log("from eval machine");' ]);
  assert_equals(stdout, 'from eval machine\n');
}, 'exit with code 0');

promise_test(async () => {
  try {
    await common.execAworker([ '--eval', '\nthrow new Error("eval machine");' ]);
    assert_unreached('unreachable');
  } catch (e) {
    assert_equals(e.code, 1);
    assert_equals(e.stdout, '');
    assert_regexp_match(e.stderr, /Error: eval machine\n\s+at eval_machine:\d+:\d+/m);
  }
}, 'error stacks');
