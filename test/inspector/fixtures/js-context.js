'use strict';

const script = new aworker.js.Script(`
(function myFunction() {
  debugger;
})();
`, {
  filename: 'my-example-script.js',
});

const ctx = new aworker.js.Context({}, {
  name: 'foobar',
  origin: 'http://my-example.com',
});

ctx.execute(script);
