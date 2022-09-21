'use strict';

const os = require('os');
const fs = require('fs');
const path = require('path');
const assert = require('assert');

const tmpdir = fs.mkdtempSync(path.join(os.tmpdir(), 'aworker'));
const repoDir = path.resolve(__dirname, '../../../');
const wptDir = process.env.TEST_WPT_DIR || path.resolve(repoDir, 'vendor/wpt');
const buildType = process.env.BUILDTYPE || 'Release';
const fixtureDirs = {
  test: path.resolve(__dirname, '..'),
  tools: path.resolve(__dirname, '../../tools'),
  tmp: path.resolve(__dirname, '../.tmp'),
  tmpdir,
  wpt: wptDir,
  product: path.resolve(repoDir, 'build/out', buildType),
  project: repoDir,
};

module.exports = {
  buildType,
  path: (type, ...fragments) => {
    const dir = fixtureDirs[type];
    assert(dir != null);
    return path.join(dir, ...fragments);
  },
};
