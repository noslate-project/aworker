'use strict';

module.exports = {
  runner: 'node',
  // Only run turf tests on linux.
  skip: process.platform !== 'linux',
};
