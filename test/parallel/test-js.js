'use strict';

function createDeferred() {
  let resolve;
  let reject;
  const promise = new Promise((_resolve, _reject) => { resolve = _resolve; reject = _reject; });
  return { promise, resolve, reject };
}

function assert_struct_equals(a, b) {
  assert_array_equals(Object.keys(a), Object.keys(b));
  for (const key of Object.keys(a)) {
    assert_equals(a[key], b[key]);
  }
}

test(() => {
  const code = 'globalThis.a = 1;';

  const { Script } = aworker.js;
  const script = new Script(code);

  assert_equals(script.filename, 'aworker.js.<anonymous>');
}, 'Script instance');

test(() => {
  const { Context } = aworker.js;
  const sandbox = {
    a: 1,
    b: 2,
  };

  const context1 = new Context(sandbox);
  assert_equals(context1.name, 'unnamed 0');
  assert_struct_equals(context1.globalThis, sandbox);
  assert_not_equals(context1.globalThis, sandbox);

  const context2 = new Context(sandbox);
  assert_equals(context2.name, 'unnamed 1');
  assert_struct_equals(context2.globalThis, sandbox);
  assert_not_equals(context2.globalThis, sandbox);
}, 'Context instance');

test(() => {
  const { Context } = aworker.js;
  const sandbox = {
    a: 1,
    b: 2,
  };
  const context = new Context(sandbox, { name: 'hello' });

  assert_equals(context.name, 'hello');
  assert_struct_equals(context.globalThis, sandbox);
  assert_not_equals(context.globalThis, sandbox);
}, 'Context instance with naming');

test(() => {
  const { Context } = aworker.js;
  const sandbox = {
    a: 1,
    b: 2,
  };
  const context = new Context(sandbox, { name: 'hello' });

  sandbox.c = 3;
  assert_struct_equals(context.globalThis, sandbox);
  assert_not_equals(context.globalThis, sandbox);
  assert_equals(context.globalThis.c, 3);
}, 'Context sandbox proxy');

test(() => {
  const { Context } = aworker.js;
  const sandbox = {
    a: 2,
  };
  const code = 'globalThis.a = 1; globalThis.plus = function plus(a, b) { return a + b; }; var abc = "hello world"; abc';
  const context = new Context(sandbox, { name: 'hello' });

  assert_equals(context.execute(code), 'hello world');
  assert_equals(sandbox.a, 1);
  assert_equals(sandbox.plus(1, 2), 3);
}, 'execute code string');

test(() => {
  const { Context } = aworker.js;
  const sandbox = {
    a: 2,
  };
  const code = 'globalThis.a = 1; globalThis.plus = function plus(a, b) { return a + b; }\nthrow new Error(`123`);';
  const context = new Context(sandbox, { name: 'hello' });

  try {
    context.execute(code);
  } catch (e) {
    assert_equals(sandbox.a, 1);
    assert_equals(sandbox.plus(1, 2), 3);

    assert_true(/^Error: 123\n.+at aworker\.js\.<anonymous>:2:7/.test(e.stack));

    return;
  }

  throw new Error('unreachable');
}, 'execute code string (throws)');

test(() => {
  const { Context, Script } = aworker.js;
  const sandbox = {
    a: 2,
  };
  const code = 'globalThis.a = 1; globalThis.plus = function plus(a, b) { return a + b; }; var abc = "ok"; abc';
  const script = new Script(code, { filename: '$hello.js' });
  const context = new Context(sandbox, { name: 'hello' });

  assert_equals(context.execute(script), 'ok');
  assert_equals(sandbox.a, 1);
  assert_equals(sandbox.plus(1, 2), 3);
}, 'execute script');

test(() => {
  const { Context, Script } = aworker.js;
  const sandbox = {
    a: 2,
  };
  const code = 'globalThis.a = 1; globalThis.plus = function plus(a, b) { return a + b; }\nthrow new Error(`123`);';
  const script = new Script(code, { filename: '$hello.js' });
  const context = new Context(sandbox, { name: 'hello' });

  try {
    context.execute(script);
  } catch (e) {
    assert_equals(sandbox.a, 1);
    assert_equals(sandbox.plus(1, 2), 3);

    assert_true(/^Error: 123\n.+at \$hello\.js:2:7/.test(e.stack));

    return;
  }

  throw new Error('unreachable');
}, 'execute script (throws)');

test(() => {
  const { Context, Script } = aworker.js;
  const sandbox = {};
  const code = 'while (1) {}';
  const script = new Script(code, { filename: '$hello.js' });
  const context = new Context(sandbox, { name: 'hello' });

  const now = Date.now();

  try {
    context.execute(script, { timeout: 1000 });
  } catch (e) {
    assert_true(e.stack.includes('Error: Script execution timed out'));
    assert_greater_than_equal(Date.now() - now, 1000);
  }
}, 'execute script timeout');

promise_test(async () => {
  const { Context, Script } = aworker.js;
  const sandbox = {};
  const code = 'var a = "hello";';
  const script = new Script(code, { filename: '$hello.js' });
  const context = new Context(sandbox, { name: 'hello' });

  const { promise, resolve } = createDeferred();

  context.execute(script, { timeout: 1000 });
  setTimeout(() => {
    // 不能有 coredump
    resolve();
  }, 1200);

  return promise;
}, 'execute script not timeout');

test(() => {
  const { Context, Script } = aworker.js;
  let sandbox = {
    a: 2,
  };
  let code = 'globalThis.a = 1;\nglobalThis.plus = function plus(a, b) { return a + b; }\nthrow new Error(`123`);\n';
  let script = new Script(code, { filename: '$hello.js', lineOffset: -2, columnOffset: 10 });
  const context = new Context(sandbox, { name: 'hello' });

  let e;
  try {
    context.execute(script);
  } catch (_e) {
    e = _e;
  }

  assert_equals(sandbox.a, 1);
  assert_equals(sandbox.plus(1, 2), 3);
  assert_true(/at \$hello\.js:1:7/.test(e.stack));

  sandbox = {
    a: 2,
  };
  code = 'throw new Error(`123`);\nglobalThis.a = 1;\nglobalThis.plus = function plus(a, b) { return a + b; }\nthrow new Error(`234`);\n';
  script = new Script(code, { filename: '$hello.js', lineOffset: 10, columnOffset: 10 });

  try {
    context.execute(script);
  } catch (_e) {
    e = _e;
  }

  assert_equals(sandbox.a, 2);
  assert_equals(sandbox.plus, undefined);
  assert_true(/^Error: 123/.test(e.stack));
  assert_true(/at \$hello\.js:11:17/.test(e.stack));
}, 'execute script lineOffset / columnOffset');

test(() => {
  const { Context, Script } = aworker.js;

  const context = new Context({});
  const code = '"bar"';
  let cachedData;
  {
    const script = new Script(code, { filename: '$hello.js' });
    assert_false(script.cachedDataConsumed);
    cachedData = script.createCachedData();
    assert_greater_than(cachedData.byteLength, 0);

    assert_equals(context.execute(script), 'bar');
  }

  {
    const script = new Script(code, { filename: '$hello.js', cachedData });
    assert_true(script.cachedDataConsumed);
    assert_equals(context.execute(script), 'bar');
  }
}, 'create cached data');

test(() => {
  const { Context, Script } = aworker.js;

  const context = new Context({});
  const code = '"bar"';
  const filename = '$hello2.js';

  // TODO(chengzhong.wcz): when creating script with same filename and cached data,
  // cachedDataConsumed can produce false positive result.

  // Cached data is invalid.
  let cachedData = new TextEncoder().encode('aba-aba');
  {
    const script = new Script(code, { filename, cachedData });
    assert_false(script.cachedDataConsumed);
    cachedData = script.createCachedData();

    assert_equals(context.execute(script), 'bar');
  }

  // Code hash not matched.
  {
    const script = new Script('"foobar"', { filename, cachedData });
    assert_false(script.cachedDataConsumed);

    assert_equals(context.execute(script), 'foobar');
  }
}, 'reject cached data');
