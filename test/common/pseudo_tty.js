'use strict';

const { NodeRunner } = require('./runner');
const fixtures = require('./fixtures');

function run(args) {
  const cp = require('child_process');
  const fs = require('fs').promises;
  const path = require('path');

  require('./node_testharness');

  promise_test(async function() {
    const filepath = args[0];
    const dirname = path.dirname(filepath);
    const basename = path.basename(filepath, '.js');
    const outFilepath = path.join(dirname, `${basename}.out`);
    let expected = await fs.readFile(outFilepath, 'utf8');
    expected = trimString(expected);

    const actual = await new Promise(resolve => {
      const child = cp.spawn(fixtures.path('product', 'aworker'), [ ...args.slice(1), filepath ], {
        env: process.env,
        stdio: 'pipe',
      });

      let out = '';
      child.stdout.on('data', chunk => { out += chunk.toString(); });
      child.stderr.on('data', chunk => { out += chunk.toString(); });
      child.on('exit', () => {
        // TODO: check exit-code by metadata
        resolve(out);
      });
    });

    try {
      assert_match(trimString(actual), expected);
    } catch (e) {
      await fs.writeFile(path.join(dirname, `${basename}.actual`), actual, 'utf8');
      throw e;
    }
  }, 'pseudo-tty test');
}

function trimString(str) {
  return str
    .split('\n')
    .map(it => it.trimEnd())
    .join('\n')
    .trimEnd();
}

function escapeRegex(string) {
  return string.replace(/[-\/\\^$*+?.()|[\]{}]/g, '\\$&');
}

function assert_match(actual, expected) {
  if (expected.includes('*')) {
    const expr = escapeRegex(expected.replace(/[*]/g, '#ANY#')).replace(/#ANY#/g, '[\\s\\S]+');
    const regexp = new RegExp(expr);
    assert_regexp_match(actual, regexp);
  } else {
    assert_equals(actual, expected);
  }
}

class PseudoTtyRunner extends NodeRunner {
  run(spec/* , scriptsToRun */) {
    const aworkerArgs = [];
    if (spec.meta.flags) {
      aworkerArgs.push(...spec.meta.flags.split(' '));
    }
    return super.spawn(spec.filename, fixtures.path('product', 'node'), [
      ...this.execArgv,
      __filename,
      'child',
      spec.getAbsolutePath(),
      ...aworkerArgs,
    ], {
      env: { ...process.env, ...spec.meta.env },
    });
  }
}

module.exports = {
  run,
  PseudoTtyRunner,
};

if (require.main === module && process.argv[2] === 'child') {
  run(process.argv.slice(3));
}
