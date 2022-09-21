'use strict';

test(() => {
  const storage = new aworker.AsyncLocalStorage();
  assert_true(storage instanceof aworker.AsyncLocalStorage);
}, 'constructor');

test(() => {
  const storage = new aworker.AsyncLocalStorage();
  let called = false;
  storage.run(1, () => {
    called = true;
    assert_equals(storage.getStore(), 1);
  });
  assert_true(called);
}, 'AsyncLocalStorage.run should invoke callback immediately');

promise_test(async () => {
  const storage = new aworker.AsyncLocalStorage();
  let promise;

  storage.run(2, () => {
    promise = Promise.resolve()
      .then(() => storage.getStore());
  });

  // run outside of scope 2.
  promise = promise.then(ret => {
    assert_equals(ret, 2);
    assert_equals(storage.getStore(), undefined);
    throw 'foobar';
  });

  storage.run(3, () => {
    promise = promise.catch(error => {
      assert_equals(error, 'foobar');
      assert_equals(storage.getStore(), 3);
    });
  });

  await promise;
}, 'AsyncLocalStorage.run with promise');

promise_test(async () => {
  const storage = new aworker.AsyncLocalStorage();

  await storage.run(2, async () => {
    const promise = Promise.resolve()
      .then(() => storage.getStore());

    const ret = await promise;
    assert_equals(ret, 2);
    assert_equals(storage.getStore(), 2);
    try {
      await Promise.reject(new Error('foobar'));
    } catch (e) {
      assert_equals(e.message, 'foobar');
      assert_equals(storage.getStore(), 2);
    }
  });

  assert_equals(storage.getStore(), undefined);
}, 'AsyncLocalStorage.run with async/await');

promise_test(async () => {
  const storage = new aworker.AsyncLocalStorage();

  async function checkStore(value) {
    assert_equals(storage.getStore(), value);
    await new Promise(resolve => setTimeout(resolve, 1));
    assert_equals(storage.getStore(), value);
  }

  await storage.run(2, async () => {
    await checkStore(2);
  });

  assert_equals(storage.getStore(), undefined);

  await checkStore(undefined);
}, 'AsyncLocalStorage.run with async/await function');
