'use strict';

// See: https://web-platform-tests.org/writing-tests/testharness-api.html
const testHarnessGlobals = [
  // Tests
  'test', 'async_test', 'promise_test',
  // Events
  'EventWatcher',
  // Timeouts in Tests
  'step_wait', 'step_wait_func', 'step_wait_func_done', 'step_timeout',
  // Setup
  'setup', 'promise_setup', 'explicit_done', 'output_document', 'explicit_timeout', 'allow_uncaught_exception', 'hide_test_state', 'timeout_multiplier', 'single_test',
  'add_start_callback', 'add_test_state_callback', 'add_result_callback', 'add_completion_callback',
  // Asserts
  'assert_true', 'assert_false', 'assert_equals', 'assert_not_equals', 'assert_in_array',
  'assert_array_equals', 'assert_array_approx_equals', 'assert_approx_equals',
  'assert_object_equals',
  'assert_less_than', 'assert_greater_than', 'assert_between_exclusive', 'assert_less_than_equal', 'assert_greater_than_equal', 'assert_between_inclusive',
  'assert_regexp_match',
  'assert_class_string', 'assert_own_property', 'assert_not_own_property', 'assert_inherits', 'assert_idl_attribute', 'assert_readonly',
  'assert_throws_dom', 'assert_throws_js', 'assert_throws_exactly',
  'assert_implements', 'assert_implements_optional',
  'assert_unreached', 'assert_any',
  // promise tests
  'promise_rejects_dom', 'promise_rejects_js', 'promise_rejects_exactly',
];

module.exports = {
  extends: '../.eslintrc.js',
  env: {
    browser: true,
    serviceworker: true,
    node: true,
    mocha: false,
  },
  globals: {
    loadBinding: false,
    load: false,
    mod: false,
    wrapper: false,
    ...testHarnessGlobals.reduce((g, key) => {
      g[key] = true;
      return g;
    }, {}),
    console: 'readonly',
    aworker: 'readonly',
    AworkerNamespace: 'readonly',
  },
};
