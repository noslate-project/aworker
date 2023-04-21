'use strict';

const common = require('../common/node_testharness');
const path = require('path');
const fixtures = require('../common/fixtures');
const fs = require('fs');

function verifyFrames(output, file, suffix) {
  const data = fs.readFileSync(file, 'utf8');
  const profile = JSON.parse(data);
  const frames = profile.nodes.filter(i => {
    const frame = i.callFrame;
    return frame.url.endsWith(suffix);
  });
  if (frames.length === 0) {
    // Show native debug output and the profile for debugging.
    console.log(output.stderr.toString());
    console.log(profile.nodes);
  }
  assert_not_equals(frames.length, 0);
}

promise_test(async () => {
  const output = await common.execAworker([ '--cpu-prof', path.join(__dirname, 'fixtures', 'fetch.js') ], {
    cwd: fixtures.path('tmp'),
    stdio: [ 'ignore', 'pipe', 'pipe' ],
  });
  const files = fs.readdirSync(fixtures.path('tmp')).filter(v => v.endsWith('.cpuprofile'));
  assert_equals(files.length, 1);
  verifyFrames(output, files[0], 'fetch.js');
}, 'should output cpuprofile file');
