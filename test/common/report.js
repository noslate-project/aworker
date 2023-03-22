'use strict';
const assert = require('assert');
const fs = require('fs');
const os = require('os');
const util = require('util');

function validate(filepath, fields, compact = false) {
  const report = fs.readFileSync(filepath, 'utf8');
  if (compact) {
    const end = report.indexOf('\n');
    assert.strictEqual(end, report.length - 1);
  }
  const content = JSON.parse(report);
  validateContent(content, fields);
  return content;
}

function validateContent(report, fields = []) {
  if (typeof report === 'string') {
    try {
      report = JSON.parse(report);
    } catch {
      throw new TypeError(
        'validateContent() expects a JSON string or JavaScript Object');
    }
  }
  try {
    _validateContent(report, fields);
  } catch (err) {
    try {
      err.stack += util.format('\n------\nFailing Report:\n%O', report);
    } catch { /** empty */ }
    throw err;
  }
}

function _validateContent(report, fields = []) {
  // Verify that all sections are present as own properties of the report.
  const sections = [ 'header', 'javascriptStack', 'nativeStack',
    'javascriptHeap', 'environmentVariables',
    'sharedObjects', 'libuv', 'resourceUsage', 'userLimits' ];

  checkForUnknownFields(report, sections);
  sections.forEach(section => {
    assert(report.hasOwnProperty(section));
    assert(typeof report[section] === 'object' && report[section] !== null);
  });

  fields.forEach(field => {
    function checkLoop(actual, rest, expect) {
      actual = actual[rest.shift()];
      if (rest.length === 0 && actual !== undefined) {
        assert.strictEqual(actual, expect);
      } else {
        assert(actual);
        checkLoop(actual, rest, expect);
      }
    }
    let actual,
      expect;
    if (Array.isArray(field)) {
      [ actual, expect ] = field;
    } else {
      actual = field;
      expect = undefined;
    }
    checkLoop(report, actual.split('.'), expect);
  });

  // Verify the format of the header section.
  const header = report.header;
  const headerFields = [ 'event', 'trigger', 'filename', 'dumpEventTime',
    'dumpEventTimeStamp', 'processId', /* TODO: 'commandLine', */
    'aworkerVersion', 'wordSize', 'arch', 'platform',
    'componentVersions', 'osName', 'osRelease',
    'osVersion', 'osMachine', /* TODO: 'cpus', */ 'host',
    'glibcVersionRuntime', 'glibcVersionCompiler', 'cwd',
    'reportVersion' ];
  checkForUnknownFields(header, headerFields);
  assert.strictEqual(header.reportVersion, 2); // Increment as needed.
  assert.strictEqual(typeof header.event, 'string');
  assert.strictEqual(typeof header.trigger, 'string');
  assert(typeof header.filename === 'string' || header.filename === null);
  assert.notStrictEqual(new Date(header.dumpEventTime).toString(),
    'Invalid Date');
  assert(String(+header.dumpEventTimeStamp), header.dumpEventTimeStamp);
  assert(Number.isSafeInteger(header.processId));
  assert.strictEqual(typeof header.cwd, 'string');

  // TODO: Validate aworkerVersion
  assert(Number.isSafeInteger(header.wordSize));
  assert.strictEqual(header.arch, os.arch());
  assert.strictEqual(header.platform, os.platform());

  const componentVersionsFields = [ 'ada', 'aworker', 'curl', 'v8', 'uv', 'zlib' ];
  assert.strictEqual(typeof header.componentVersions, 'object');
  checkForUnknownFields(header.componentVersions, componentVersionsFields);

  assert.strictEqual(header.osName, os.type());
  assert.strictEqual(header.osRelease, os.release());
  assert.strictEqual(typeof header.osVersion, 'string');
  assert.strictEqual(typeof header.osMachine, 'string');
  assert.strictEqual(header.host, os.hostname());

  // Verify the format of the javascriptStack section.
  checkForUnknownFields(report.javascriptStack,
    [ 'message', 'stack', 'errorProperties' ]);
  assert.strictEqual(typeof report.javascriptStack.errorProperties,
    'object');
  assert.strictEqual(typeof report.javascriptStack.message, 'string');
  if (report.javascriptStack.stack !== undefined) {
    assert(Array.isArray(report.javascriptStack.stack));
    report.javascriptStack.stack.forEach(frame => {
      assert.strictEqual(typeof frame, 'string');
    });
  }

  // Verify the format of the nativeStack section.
  assert(Array.isArray(report.nativeStack));
  report.nativeStack.forEach(frame => {
    assert(typeof frame === 'object' && frame !== null);
    checkForUnknownFields(frame, [ 'pc', 'symbol' ]);
    assert.strictEqual(typeof frame.pc, 'string');
    assert(/^0x[0-9a-f]+$/.test(frame.pc));
    assert.strictEqual(typeof frame.symbol, 'string');
  });

  // Verify the format of the javascriptHeap section.
  const heap = report.javascriptHeap;
  const jsHeapFields = [ 'totalMemory', 'totalCommittedMemory', 'usedMemory',
    'availableMemory', 'memoryLimit', 'heapSpaces' ];
  checkForUnknownFields(heap, jsHeapFields);
  assert(Number.isSafeInteger(heap.totalMemory));
  assert(Number.isSafeInteger(heap.totalCommittedMemory));
  assert(Number.isSafeInteger(heap.usedMemory));
  assert(Number.isSafeInteger(heap.availableMemory));
  assert(Number.isSafeInteger(heap.memoryLimit));
  assert(typeof heap.heapSpaces === 'object' && heap.heapSpaces !== null);
  const heapSpaceFields = [ 'memorySize', 'committedMemory', 'capacity', 'used',
    'available' ];
  Object.keys(heap.heapSpaces).forEach(spaceName => {
    const space = heap.heapSpaces[spaceName];
    checkForUnknownFields(space, heapSpaceFields);
    heapSpaceFields.forEach(field => {
      assert(Number.isSafeInteger(space[field]));
    });
  });

  assert(Array.isArray(report.libuv));
  report.libuv.forEach(handle => {
    assert.strictEqual(typeof handle, 'object');
    assert.strictEqual(typeof handle.type, 'string');
    assert.strictEqual(typeof handle.is_active, 'boolean');
    assert.strictEqual(typeof handle.address, 'string');
  });

  // Verify the format of the resourceUsage section.
  const usage = report.resourceUsage;
  const resourceUsageFields = [ 'userCpuSeconds', 'kernelCpuSeconds',
    'cpuConsumptionPercent', 'maxRss',
    'pageFaults', 'fsActivity' ];
  checkForUnknownFields(usage, resourceUsageFields);
  assert.strictEqual(typeof usage.userCpuSeconds, 'number');
  assert.strictEqual(typeof usage.kernelCpuSeconds, 'number');
  assert.strictEqual(typeof usage.cpuConsumptionPercent, 'number');
  assert(Number.isSafeInteger(usage.maxRss));
  assert(typeof usage.pageFaults === 'object' && usage.pageFaults !== null);
  checkForUnknownFields(usage.pageFaults, [ 'IORequired', 'IONotRequired' ]);
  assert(Number.isSafeInteger(usage.pageFaults.IORequired));
  assert(Number.isSafeInteger(usage.pageFaults.IONotRequired));
  assert(typeof usage.fsActivity === 'object' && usage.fsActivity !== null);
  checkForUnknownFields(usage.fsActivity, [ 'reads', 'writes' ]);
  assert(Number.isSafeInteger(usage.fsActivity.reads));
  assert(Number.isSafeInteger(usage.fsActivity.writes));

  // Verify the format of the environmentVariables section.
  for (const [ key, value ] of Object.entries(report.environmentVariables)) {
    assert.strictEqual(typeof key, 'string');
    assert.strictEqual(typeof value, 'string');
  }

  // Verify the format of the userLimits section on non-Windows platforms.
  const userLimitsFields = [ 'core_file_size_blocks', 'data_seg_size_kbytes',
    'file_size_blocks', 'max_locked_memory_bytes',
    'max_memory_size_kbytes', 'open_files',
    'stack_size_bytes', 'cpu_time_seconds',
    'max_user_processes', 'virtual_memory_kbytes' ];
  checkForUnknownFields(report.userLimits, userLimitsFields);
  for (const [ type, limits ] of Object.entries(report.userLimits)) {
    assert.strictEqual(typeof type, 'string');
    assert(typeof limits === 'object' && limits !== null);
    checkForUnknownFields(limits, [ 'soft', 'hard' ]);
    assert(typeof limits.soft === 'number' || limits.soft === 'unlimited',
      `Invalid ${type} soft limit of ${limits.soft}`);
    assert(typeof limits.hard === 'number' || limits.hard === 'unlimited',
      `Invalid ${type} hard limit of ${limits.hard}`);
  }

  // Verify the format of the sharedObjects section.
  assert(Array.isArray(report.sharedObjects));
  report.sharedObjects.forEach(sharedObject => {
    assert.strictEqual(typeof sharedObject, 'string');
  });
}

function checkForUnknownFields(actual, expected) {
  Object.keys(actual).forEach(field => {
    assert(expected.includes(field), `'${field}' not expected in ${expected}`);
  });
}

module.exports = { validate, validateContent };
