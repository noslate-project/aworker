// META: flags=--expose-internals
'use strict';

const { setImmediate, clearImmediate } = load('timer');

setImmediate((...args) => { console.log(...args); }, '3', '3');
const id = setImmediate(() => { console.log('will never print'); });

console.log('1');

Promise.resolve().then(() => console.log('2'));

clearImmediate(id);
