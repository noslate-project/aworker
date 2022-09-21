'use strict';

const { hrtime, getTimeOrigin } = loadBinding('perf');

function now() {
  return Number(hrtime() - getTimeOrigin()) / 1e6;
}

wrapper.mod = {
  now,
};
