'use strict';

const { Console } = load('console/console');

function createGlobalConsole() {
  const console = new Console();
  const globalConsole = Object.create({});
  for (const prop of Reflect.ownKeys(console)) {
    const desc = Reflect.getOwnPropertyDescriptor(console, prop);
    Reflect.defineProperty(globalConsole, prop, desc);
  }
  return globalConsole;
}

mod.console = createGlobalConsole();
