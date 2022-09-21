'use strict';

const fs = require('fs');
const vm = require('vm');
const fixtures = require('./fixtures');

function getBuildConfig() {
  const configGypiPath = fixtures.path('project', 'build/config.gypi');
  const content = fs.readFileSync(configGypiPath, 'utf8');
  const result = vm.runInNewContext('it = ' + content + '; it');
  return result.variables;
}

module.exports = {
  getBuildConfig,
};
