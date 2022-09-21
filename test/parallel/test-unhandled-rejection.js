'use strict';
setup({
  allow_uncaught_exception: true,
});

const t = async_test('"unhandledrejection" event on unhandled promise rejection');
let promise;

addEventListener('unhandledrejection', t.step_func(event => {
  assert_equals(event.type, 'unhandledrejection');
  assert_equals(event.reason.message, 'foobar');
  event.preventDefault();

  // trigger rejectionhandled event
  promise.catch(() => {});
}));

addEventListener('rejectionhandled', t.step_func_done(event => {
  assert_equals(event.type, 'rejectionhandled');
  assert_equals(event.reason.message, 'foobar');
}));

promise = new Promise((_, reject) => {
  setTimeout(() => {
    reject(new Error('foobar'));
  }, 1);
});
