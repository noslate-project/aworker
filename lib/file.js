'use strict';

const file = loadBinding('file');

const {
  TextDecoder,
} = load('encoding');

function readFile(filename, encoding = 'binary') {
  if (typeof filename !== 'string') {
    throw TypeError('filename should be a string.');
  }

  const content = file.readFile(filename);
  switch (encoding.toLowerCase()) {
    case 'binary': return content;
    default: {
      const decoder = new TextDecoder(encoding);
      return decoder.decode(content);
    }
  }
}

wrapper.mod = {
  readFile,
};
