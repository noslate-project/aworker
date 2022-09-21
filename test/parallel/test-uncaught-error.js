'use strict';
setup({
  allow_uncaught_exception: true,
});

const t = async_test('"error" event on uncaught error');

addEventListener('error', t.step_func_done(event => {
  assert_equals(event.type, 'error');
  assert_equals(event.message, 'foobar');

  event.preventDefault();
}));

// In test suites, no error should be thrown in the top level frame.
// Or else there will be no `install` event to trigger testharness completion.
queueMicrotask(() => {
  throw new Error('foobar');
});
