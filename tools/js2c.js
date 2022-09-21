'use strict';

const fs = require('fs');
const path = require('path');
const assert = require('assert');
const { TextEncoder } = require('util');

function main(argv) {
  const ROOT = path.resolve(__dirname, '..', 'lib');
  const bytes = new Map();
  const encoder = new TextEncoder();
  const filesToEncode = [];
  let outFile;

  while (argv.length > 0) {
    let arg;
    switch (arg = argv.shift()) {
      default: {
        if (outFile == null) {
          outFile = arg;
        } else {
          filesToEncode.push(arg);
        }
      }
    }
  }

  assert(outFile != null, 'Expect an output file path to be the first argument.');

  for (const file of filesToEncode) {
    const relPath = path.relative(ROOT, file);
    if (relPath.startsWith('..')) {
      throw new Error(`Unexpected lib file outside JavaScript lib root dir: ${file}`);
    }
    const content = fs.readFileSync(file, 'utf8');
    bytes.set(relPath, content);
  }

  let source = `#include "native_module_manager.h"

  namespace aworker {
  namespace NativeJavaScript {

  `;

  for (const fn of bytes.keys()) {
    const rawName = `${fn.replace(/[\/\.]/g, '_')}_raw`;
    source += `const uint8_t ${rawName}[] = {`;

    const content = bytes.get(fn);
    const encoded = encoder.encode(content);
    for (const [ idx, code ] of encoded.entries()) {
      assert.ok(code > 0 && code < 256, `Byte code must be positive char type, but got ${code}`);
      if (idx !== 0) {
        source += ',';
      }

      if (idx % 30 === 0) {
        source += '\n';
      }

      source += ` ${code}`;
    }

    source += ',0\n};\n\n';
  }

  source += `
}  // namespace NativeJavaScript
`;

  source += `
  void NativeModuleManager::InitializeNativeModuleSource() {
  `;

  for (const fn of bytes.keys()) {
    const rawName = `${fn.replace(/[\/\.]/g, '_')}_raw`;
    source += `  native_module_source_map_["${fn}"] = NativeJavaScript::${rawName};\n`;
  }

  source += `}

  }  // namespace aworker
  `;

  fs.writeFileSync(outFile, source, 'utf8');
  console.log(`[OK] 👶 ${outFile} generated.`);
}

if (require.main === module) {
  main(process.argv.slice(2));
}
