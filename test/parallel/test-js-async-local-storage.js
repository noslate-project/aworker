'use strict';

promise_test(async () => {
  const { Context } = aworker.js;
  const storage = new aworker.AsyncLocalStorage();

  await storage.run(1, async () => {
    const sandbox = {};
    const code = `
    async function that() {
      await Promise.resolve();
    }

    that();
`;
    const context = new Context(sandbox, { name: 'hello' });
    const value = await context.execute(code)
      .then(() => storage.getStore());

    assert_equals(value, 1);
    assert_equals(storage.getStore(), 1);
  });

  assert_equals(storage.getStore(), undefined);
}, 'tracking cross context promises');
