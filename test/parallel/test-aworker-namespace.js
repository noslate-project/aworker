// META: env.FOO=BAR

'use strict';

test(() => {
  assert_true(aworker instanceof AworkerNamespace);
}, 'aworker prototype');

test(() => {
  assert_throws_js(TypeError, () => {
    new AworkerNamespace();
  });
}, 'constructor');

test(() => {
  assert_equals(typeof aworker, 'object');
  assert_equals(typeof aworker.env, 'object');
  assert_equals(aworker.env.FOO, 'BAR');

  aworker.env.FOO = 'QUX';
  assert_equals(aworker.env.FOO, 'QUX');
}, 'AworkerNamespace.env getter/setter');

test(() => {
  assert_equals(typeof aworker, 'object');
  assert_equals(typeof aworker.arch, 'string');
}, 'AworkerNamespace.arch');

test(() => {
  assert_equals(typeof aworker, 'object');
  assert_equals(typeof aworker.platform, 'string');
}, 'AworkerNamespace.platform');

test(() => {
  assert_regexp_match(aworker.version, /v\d+\.\d+\.\d+(?:-[0-9a-f]{6})?/);
}, 'AworkerNamespace.version');

test(() => {
  assert_equals(typeof aworker.versions, 'object');
  for (const key of Object.keys(aworker.versions)) {
    assert_equals(typeof aworker.versions[key], 'string');
  }
}, 'AworkerNamespace.versions');
