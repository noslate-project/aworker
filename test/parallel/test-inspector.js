'use strict';

test(() => {
  assert_throws_js(TypeError, () => aworker.inspector.callAndPauseOnFirstStatement(1));
  assert_throws_js(TypeError, () => aworker.inspector.callAndPauseOnFirstStatement(() => {}));
}, 'inspector.callAndPauseOnFirstStatement should throw if the argument is invalid');

test(() => {
  assert_false(aworker.inspector.isActive());
  assert_throws_js(TypeError, () => aworker.inspector.callAndPauseOnFirstStatement(() => {}, globalThis));
}, 'inspector.callAndPauseOnFirstStatement should throw when inspector is not active');
