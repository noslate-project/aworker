'use strict';

const path = require('path');
const common = require('../common/node_testharness');

promise_test(async function() {
  const std = await common.execAworker([ '--enable-source-maps', path.join(__dirname, 'fixtures', 'inline-source-maps.js') ]).catch(e => e);
  assert_regexp_match(std.stderr, /Error: foo\n\s+at main \(.+\/test\/node_parallel\/fixtures\/inline-source-maps.js:3:15\)\n\s+-> .+\/test\/node_parallel\/report.ts:4:13/);
}, 'uncaught exceptions');
