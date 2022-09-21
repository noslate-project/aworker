/* eslint-env node */
'use strict';

module.exports = {
  extends: '../.eslintrc.js',
  env: {
    browser: true,
    serviceworker: true,
    node: false,
    mocha: false,
  },
  globals: {
    createBenchmark: 'readonly',
    aworker: 'readonly',
    AworkerNamespace: 'readonly',
  },
};
