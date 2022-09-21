'use strict';
/* global module */

module.exports = {
  extends: '../build/.eslintrc.js',
  env: {
    browser: false,
    node: false,
    serviceworker: false,
  },
  globals: {
    loadBinding: 'readonly',
    load: 'readonly',
    mod: true,
    wrapper: true,
  },
};
