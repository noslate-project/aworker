'use strict';
setup({
  allow_uncaught_exception: true,
});

const t = async_test('"error" event on uncaught error in async execution context');

addEventListener('error', t.step_func_done(event => {
  assert_equals(event.type, 'error');
  assert_equals(event.message, 'foobar');

  event.preventDefault();
}));

setTimeout(() => {
  throw new Error('foobar');
}, 1);
