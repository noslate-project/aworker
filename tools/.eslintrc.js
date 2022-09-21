'use strict';

module.exports = {
  extends: '../.eslintrc.js',
  env: {
    node: true,
  },
  globals: {
    loadBinding: false,
    load: false,
    mod: false,
    wrapper: false,
  },
};
