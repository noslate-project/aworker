'use strict';

class Foo {
  constructor() {
    this.bar = 'x'.repeat(10000);
  }
}

for (let i = 0; i <= 100000; i++) {
  console.log(new Foo());
}