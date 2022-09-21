'use strict';
setup({
  allow_uncaught_exception: true,
});

const t = async_test('"unhandledrejection" event on unhandled promise rejection');
let promise_chained;

addEventListener('unhandledrejection', t.step_func(event => {
  assert_equals(event.type, 'unhandledrejection');
  assert_equals(event.reason.message, 'qux');
  assert_equals(event.promise, promise_chained);
  event.preventDefault();

  // trigger rejectionhandled event
  promise_chained.catch(() => {});
}));

addEventListener('rejectionhandled', t.step_func_done(event => {
  assert_equals(event.type, 'rejectionhandled');
  assert_equals(event.reason.message, 'qux');
  assert_equals(event.promise, promise_chained);
}));

// In place constructed rejected promise should not trigger unhandledrejection
// if they are handled in the same execution frame
const promise = Promise.reject(new Error('foobar'));
promise_chained = promise.then(() => {}, e => {
  assert_equals(e.message, 'foobar');
  throw new Error('qux');
});
