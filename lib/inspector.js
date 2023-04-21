'use strict';

const {
  isActive,
  callAndPauseOnFirstStatement,
} = loadBinding('inspector');

wrapper.mod = {
  isActive,
  callAndPauseOnFirstStatement,
};
