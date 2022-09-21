'use strict';

const { Context, Script } = aworker.js;
const sandbox = {};
const code = 'while (1) {}';
const script = new Script(code, { filename: '$hello.js' });
const context = new Context(sandbox, { name: 'hello' });

context.execute(script);
