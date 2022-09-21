'use strict';

const process = loadBinding('process');

function exit(code) {
  process.exit(code);
}

wrapper.mod = {
  publicExit: exit,
};
