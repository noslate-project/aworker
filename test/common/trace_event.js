'use strict';

const path = require('path');
const fs = require('fs');

module.exports = {
  findTraceFile,
  readTraceFile,
};

function findTraceFile(cwd, pid) {
  let filepath;
  if (pid == null) {
    const files = fs.readdirSync(cwd);
    filepath = path.join(cwd, files.find(it => it.startsWith('aworker_trace') && it.endsWith('.log')));
  } else {
    filepath = path.join(cwd, `aworker_trace.${pid}.1.log`);
  }
  try {
    const stat = fs.statSync(filepath);
    if (!stat.isFile()) {
      return false;
    }
    return filepath;
  } catch (e) {
    if (e.code === 'ENOENT') {
      return false;
    }
    throw e;
  }
}

function readTraceFile(cwd, pid) {
  const filepath = findTraceFile(cwd, pid);
  assert_equals(typeof filepath, 'string', 'trace file not found');
  const contentStr = fs.readFileSync(filepath, 'utf8');
  const content = parseTraceFile(contentStr);
  return content;
}

function parseTraceFile(content) {
  // incomplete flushing..
  if (!content.endsWith(']}')) {
    content += ']}';
  }
  return JSON.parse(content);
}
