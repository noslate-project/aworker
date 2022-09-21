'use strict';

test(() => {
  assert_regexp_match(location.href, /^file:\/\/\/.+\/test-location.js$/);
}, 'location default value');
