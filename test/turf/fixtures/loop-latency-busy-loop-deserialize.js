'use strict';

function busyLoop() {
  const startMs = Date.now();
  while (true) {
    console.log('busy loop at ' + (Date.now() - startMs) + 'ms');
  }
}

addEventListener('deserialize', () => {
  busyLoop();
});
