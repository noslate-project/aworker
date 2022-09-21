'use strict';

console.log('first frame');
addEventListener('deserialize', () => {
  console.log('deserialize');
});
addEventListener('install', () => {
  console.log('install');
});
