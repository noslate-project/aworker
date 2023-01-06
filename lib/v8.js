'use strict';

const v8 = loadBinding('v8');

function writeHeapSnapshot(filename) {
  if (typeof filename !== 'string') {
    throw new TypeError(`Invalid filename type: ${typeof filename}.`);
  }

  v8.writeHeapSnapshot(filename);
}

wrapper.mod = {
  writeHeapSnapshot,
  getHeapSnapshot: v8.getHeapSnapshot,
  getTotalHeapSize: v8.getTotalHeapSize,
};
