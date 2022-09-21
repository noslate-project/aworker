'use strict';

const { structuredClone } = loadBinding('serdes');

test(() => {
  const obj = structuredClone({
    foo: {
      bar: 'foobar',
    },
  });
  assert_equals(obj.foo.bar, 'foobar');
}, 'structured clone object');

test(() => {
  const maybe_null = structuredClone(null);
  assert_equals(maybe_null, null);
}, 'structured clone null');

test(() => {
  assert_throws_dom('DataCloneError', () => {
    structuredClone({ unserializable: Symbol() });
  });
}, 'structured clone unserializable');
