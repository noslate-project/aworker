'use strict';

const t = async_test('microtasks');

(async function() {
  const res = await step();
  assert_true(res);
  t.done();
})();

async function step() {
  return true;
}
