'use strict';

const http = require('http');
const childProcess = require('child_process');
const readline = require('readline');

const fixtures = require('../common/fixtures');
const { sleep } = require('../common/node_testharness');

async function run(scriptPath, breakOnEntry = true) {
  const cp = await new Promise(resolve => {
    const cp = childProcess.spawn(
      fixtures.path('product', 'node'),
      [
        fixtures.path('tools', 'shell.js'),
        '--bin', fixtures.path('product', 'aworker'),
        '--agent-server-path', fixtures.path('tmpdir', 'noslated.sock'),
        breakOnEntry ? '--inspect-brk' : '--inspect',
        scriptPath,
      ],
      {
        env: {
          ...process.env,
          NODE_PATH: fixtures.path('project'),
        },
        stdio: [ 'ignore', 'pipe', 'pipe' ],
      }
    );
    const stdout = readline.createInterface(cp.stdout);
    const stderr = readline.createInterface(cp.stderr);
    stdout.on('line', line => {
      if (!breakOnEntry && line === 'Agent Connected.') {
        resolve(cp);
      }
      console.log('child:', line);
    });
    stderr.on('line', line => {
      if (breakOnEntry && line.startsWith('Waiting for the debugger to connect')) {
        resolve(cp);
      }
      console.error('child:', line);
    });
    cp.on('exit', (code, signal) => {
      console.log('worker exited', code, signal);
    });
  });

  for (let times = 0; times < 3; times++) {
    const targets = await fetchJSON('http://localhost:9229/json');
    if (Array.isArray(targets) && targets.length) {
      break;
    }

    await sleep(256);
  }

  return cp;
}

function fetchJSON(url) {
  return new Promise((resolve, reject) => {
    const httpReq = http.get(url);

    const chunks = [];

    function onResponse(httpRes) {
      function parseChunks() {
        const resBody = Buffer.concat(chunks).toString();
        if (httpRes.statusCode !== 200) {
          reject(new Error(`Unexpected ${httpRes.statusCode}: ${resBody}`));
          return;
        }
        try {
          resolve(JSON.parse(resBody));
        } catch (parseError) {
          reject(new Error(`Response didn't contain JSON: ${resBody}`));
          return;
        }
      }

      httpRes.on('error', reject);
      httpRes.on('data', chunk => chunks.push(chunk));
      httpRes.on('end', parseChunks);
    }

    httpReq.on('error', reject);
    httpReq.on('response', onResponse);
  });
}


module.exports = {
  run,
};
