/* eslint-env node */
'use strict';

const CLI = require('./_cli.js');
const Runner = require('./_run');

const cli = new CLI(`usage: ./node run.js [options] [--] <category> ...
  Run each benchmark in the <category> directory a single time, more than one
  <category> directory can be specified.

  --filter   pattern        includes only benchmark scripts matching <pattern>
                            (can be repeated)
  --exclude  pattern        excludes scripts matching <pattern> (can be
                            repeated)
  --set    variable=value   set benchmark variable (can be repeated)
  test                      only run a single configuration from the options
                            matrix
  all                       each benchmark category is run one after the other
`, { arrayArgs: [ 'set', 'filter', 'exclude' ] });
const benchmarks = cli.benchmarks();

if (benchmarks.length === 0) {
  console.error('No benchmarks found');
  process.exitCode = 1;
  return;
}

const validFormats = [ 'csv', 'simple' ];
const format = cli.optional.format || 'simple';
if (!validFormats.includes(format)) {
  console.error('Invalid format detected');
  process.exitCode = 1;
  return;
}

if (format === 'csv') {
  console.log('"filename", "configuration", "rate", "time"');
}

(async function recursive(i) {
  const filename = benchmarks[i];

  const runner = new Runner(filename, {
    test: cli.test,
    set: cli.optional.set,
  });
  await runner.run();

  if (i < benchmarks.length - 1) {
    recursive(i + 1);
  }
})(0);
