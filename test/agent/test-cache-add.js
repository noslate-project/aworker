// META: same-origin-shared-data=true
'use strict';

const url = 'http://localhost:30122/dump';
const preset = [
  url,
  `${url}?foobar`,
];
function preset_add_test(name, query, options, expect) {
  promise_test(async () => {
    await caches.delete('v1');
    const cache = await caches.open('v1');
    for (const req of preset) {
      await cache.add(req);
    }
    const responses = await cache.matchAll(query, options);
    await expect(responses);
  }, `Cache.prototype.add: ${name}`);
}
function preset_add_all_test(name, query, options, expect) {
  promise_test(async () => {
    await caches.delete('v1');
    const cache = await caches.open('v1');
    await cache.addAll(preset);
    const responses = await cache.matchAll(query, options);
    await expect(responses);
  }, `Cache.prototype.addAll: ${name}`);
}

preset_add_test('plain match', url, {}, async responses => {
  assert_equals(responses.length, 1, 'cache matched');
  const response = responses[0];
  assert_equals(response.url, url);
  assert_equals(response.status, 200);
  const bodyParsed = await response.json();
  assert_equals(bodyParsed.method, 'GET');
  assert_equals(bodyParsed.url, '/dump');
});
preset_add_test('match with ignoreSearch', url, { ignoreSearch: true }, async responses => {
  assert_equals(responses.length, 2, 'cache matched');
  const response = responses[1];
  assert_equals(response.url, `${url}?foobar`);
  assert_equals(response.status, 200);
  const bodyParsed = await response.json();
  assert_equals(bodyParsed.url, '/dump?foobar');
});

preset_add_all_test('plain match', url, {}, async responses => {
  assert_equals(responses.length, 1, 'cache matched');
  const response = responses[0];
  assert_equals(response.url, url);
  assert_equals(response.status, 200);
  const bodyParsed = await response.json();
  assert_equals(bodyParsed.method, 'GET');
  assert_equals(bodyParsed.url, '/dump');
});
