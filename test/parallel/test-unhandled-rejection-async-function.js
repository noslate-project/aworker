'use strict';
setup({
  allow_uncaught_exception: true,
});

const t = async_test('"unhandledrejection" event on async function');

let promise_chained;
function registerEventHandler() {
  addEventListener('unhandledrejection', t.step_func(event => {
    assert_equals(event.type, 'unhandledrejection');
    assert_equals(event.reason.message, 'foobar');
    assert_equals(event.promise, promise_chained);
    event.preventDefault();

    // trigger rejectionhandled event
    promise_chained.catch(() => {});
  }));

  addEventListener('rejectionhandled', t.step_func_done(event => {
    assert_equals(event.type, 'rejectionhandled');
    assert_equals(event.reason.message, 'foobar');
    assert_equals(event.promise, promise_chained);
  }));
}

async function test() {
  return Promise.reject(new Error('foobar'));
}

async function main() {
  try {
    await test();
  } catch (e) {
    assert_equals(e.message, 'foobar');
  }

  registerEventHandler();
  promise_chained = test();
}

main();
