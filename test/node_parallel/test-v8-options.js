'use strict';

require('../common/node_testharness');

const cp = require('child_process');
const path = require('path');

const fixtures = require('../common/fixtures');

promise_test(async function() {
  return new Promise((resolve, reject) => {
    const child = cp.spawn(fixtures.path('product', 'aworker'), [ '--v8-options' ], {
      env: process.env,
      stdio: [ 'ignore', 'pipe', 'pipe' ],
    });

    let stdout = '';
    let stderr = '';
    child.stdout.on('data', chunk => { stdout += chunk.toString(); });
    child.stderr.on('data', chunk => { stderr += chunk.toString(); });
    child.on('close', (code, signal) => {
      try {
        assert_true(stdout.includes('--use-strict'));
        assert_equals(stderr, '');
        assert_equals(code, 0);
        assert_equals(signal, null);
      } catch (e) {
        return reject(e);
      }

      resolve();
    });
  });
}, 'should output --v8-options');

promise_test(async function() {
  const filename = path.join(__dirname, 'fixtures/heap-exceeded.js');
  return new Promise((resolve, reject) => {
    const child = cp.spawn(fixtures.path('product', 'aworker'), [ '--max-heap-size=5', filename ], {
      stdio: [ 'ignore', 'pipe', 'pipe' ],
    });

    let stdout = '';
    let stderr = '';
    child.stdout.on('data', chunk => { stdout += chunk.toString(); });
    child.stderr.on('data', chunk => { stderr += chunk.toString(); });
    child.on('close', (number, signal) => {
      try {
        assert_equals(stdout, '');
        assert_true(stderr.includes('<--- Last few GCs --->'));
        assert_equals(number, null);
        assert_equals(signal, 'SIGABRT');
      } catch (e) {
        return reject(e);
      }

      resolve();
    });

  });
}, 'max-heap-size');
