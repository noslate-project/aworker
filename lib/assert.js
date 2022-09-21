'use strict';

class AssertionError extends Error {
  constructor(msg) {
    super(msg);
    this.name = 'AssertionError';
  }
}

function assert(cond, msg = 'Assertion failed.') {
  if (!cond) {
    throw new AssertionError(msg);
  }
}

wrapper.mod = {
  AssertionError,
  assert,
};
