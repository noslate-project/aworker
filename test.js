const url = 'http://localhost:12345';
const fetchPromises = [];
for (let i = 0; i < 1; i++) {
  fetchPromises.push(fetch(url));
}

Promise.all(fetchPromises).then(() => {
  const entries = performance.getEntries();

  console.log('============');
  console.log(entries);
  console.log(entries.length);
});