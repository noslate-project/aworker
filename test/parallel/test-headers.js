// META: location=http://localhost:30122
// META: flags=--experimental-curl-fetch
'use strict';

test(() => {
  const headers1 = new Headers({
    foo: 'bar',
    maybedelete: 'yes',
  });
  const headers2 = new Headers(headers1);
  headers1.delete('maybeDelete');
  headers1.set('foo', 'quz');
  headers1.append('foo', 'quz');
  assert_equals(headers2.get('maybedelete'), 'yes');
  assert_equals(headers2.get('foo'), 'bar');
}, 'copied headers should not be modified if the original one is modified');
