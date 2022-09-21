'use strict';

const fs = require('fs');

const _ = require('lodash');

const { flag, int, string, file } = require('../src/command_parser-options.json');

const implies = {};
let output = `#ifndef SRC_GEN_COMMAND_PARSER_OPTIONS_H_
#define SRC_GEN_COMMAND_PARSER_OPTIONS_H_

`;

function extractFunc(name) {
  return _.upperFirst(_.camelCase(name));
}

function extractName(key, item) {
  return item.var || _.snakeCase(key);
}

function extractCommon(container, key) {
  const item = container[key];
  const name = extractName(key, item);
  const func = extractFunc(name);
  return { item, name, func };
}

function findName(key) {
  const item = flag[key] || int[key] || string[key] || file[key];
  if (!item) {
    throw new Error(`No '${key}' defined in command_parser-options.json.`);
  }
  return extractName(key, item);
}

output += '#define CLI_LIT_ARGS(V)';
for (const key in flag) {
  if (!flag.hasOwnProperty(key)) continue;

  const { item, name, func } = extractCommon(flag, key);
  const { desc, short } = item;

  if (typeof desc !== 'string') {
    const msg = `invalid --${key} description.`;
    throw new Error(msg);
  }

  if (item.imply) {
    implies[name] = findName(item.imply);
  }

  output += ` \\
  V(${name}, ${JSON.stringify(key)}, ${short ? JSON.stringify(short) : 'nullptr'}, ${JSON.stringify(desc)}, ${func})`;
}

output += `

#define CLI_INT_ARGS(V)`;
for (const key in int) {
  if (!int.hasOwnProperty(key)) continue;

  const { item, name, func } = extractCommon(int, key);
  const { desc, meta, default: dflt } = item;

  if (typeof desc !== 'string' || typeof meta !== 'string' || typeof dflt !== 'number') {
    const msg = `invalid --${key} description / meta / default`;
    throw new Error(msg);
  }

  if (item.imply) {
    implies[name] = findName(item.imply);
  }

  output += ` \\
  V(${name}, ${JSON.stringify(key)}, ${JSON.stringify(meta)}, ${JSON.stringify(desc)}, ${dflt}, ${func})`;
}

output += `

#define CLI_STRN_ARGS(V)`;
for (const key in string) {
  if (!string.hasOwnProperty(key)) continue;

  const { item, name, func } = extractCommon(string, key);
  const { desc, meta, default: dflt } = item;

  if (typeof desc !== 'string' || typeof meta !== 'string') {
    const msg = `invalid --${key} description / meta`;
    throw new Error(msg);
  }

  if (item.imply) {
    implies[name] = findName(item.imply);
  }

  output += ` \\
  V(${name}, ${JSON.stringify(key)}, ${JSON.stringify(meta)}, 0, 1, ${JSON.stringify(desc)}, ${JSON.stringify(dflt || '')}, ${func})`;
}

// **SCRIPT FILENAME**!!!
output += `\\
  V(script_filename, \\
    nullptr, \\
    "<SCRIPT|FILENAME>", \\
    0, \\
    1, \\
    "script to be run", \\
    "", \\
    ScriptFilename)`;


output += `

#define CLI_FILEN_ARGS(V)`;
for (const key in file) {
  if (!file.hasOwnProperty(key)) continue;

  const { item, name, func } = extractCommon(file, key);
  const { desc, meta } = item;

  if (typeof desc !== 'string' || typeof meta !== 'string') {
    const msg = `invalid --${key} description / meta`;
    throw new Error(msg);
  }

  if (item.imply) {
    implies[name] = findName(item.imply);
  }

  output += ` \\
  V(${name}, ${JSON.stringify(key)}, ${JSON.stringify(meta)}, 0, 1, ${JSON.stringify(desc)}, ${func})`;
}

output += `

`;

// Implies
output += `

#define CLI_IMPLIES(V)`;
for (const key in implies) {
  if (!implies.hasOwnProperty(key)) continue;
  output += `\\
    V(${key}, ${implies[key]})`;
}

output += `

#endif  // SRC_GEN_COMMAND_PARSER_OPTIONS_H_
`;

const outputFile = process.argv[2];

fs.writeFileSync(outputFile, output, 'utf8');
console.log(`[OK] 👶 ${outputFile} generated.`);
