// META: same-origin-shared-data=true
'use strict';

const preset = [
  [ 'http://foobar', new Response('http://foobar') ],
  [ 'http://foobar/with-search?foo=bar', new Response('http://foobar/with-search?foo=bar') ],
  [ 'http://foobar/with-search', new Response('http://foobar/with-search') ],
  [ 'http://foobar/with-hash#hash', new Response('http://foobar/with-hash#hash') ],
  [ new Request('http://foobar/vary-ua', { headers: { ua: 'mod' } }), new Response('mod', { headers: { vary: 'ua' } }) ],
  [ new Request('http://foobar/vary-ua', { headers: { ua: 'mod-2' } }), new Response('mod-2', { headers: { vary: 'ua' } }) ],
];
function preset_match_test(name, query, options, expect) {
  promise_test(async () => {
    await caches.delete('v1');
    const cache = await caches.open('v1');
    for (const [ req, resp ] of preset) {
      await cache.put(req, resp);
    }
    const resp = await cache.match(query, options);
    await expect(resp);
  }, `Cache.prototype.match: ${name}`);
}

function preset_match_all_test(name, query, options, expect) {
  promise_test(async () => {
    await caches.delete('v1');
    const cache = await caches.open('v1');
    for (const [ req, resp ] of preset) {
      await cache.put(req, resp);
    }
    const responses = await cache.matchAll(query, options);
    await expect(responses);
  }, `Cache.prototype.matchAl: ${name}`);
}

preset_match_test('plain match', 'http://foobar', {}, async resp => {
  assert_true(resp != null, 'cache matched');
  assert_equals(await resp.text(), 'http://foobar');
});
preset_match_test('match with ignoreSearch', 'http://foobar/with-search', { ignoreSearch: true }, async resp => {
  assert_true(resp != null, 'cache matched');
  assert_equals(await resp.text(), 'http://foobar/with-search?foo=bar');
});
preset_match_test('match with hash', 'http://foobar/with-hash', {}, async resp => {
  assert_true(resp != null, 'cache matched');
  assert_equals(await resp.text(), 'http://foobar/with-hash#hash');
});
preset_match_test('match with vary matching', new Request('http://foobar/vary-ua', { headers: { ua: 'mod' } }), {}, async resp => {
  assert_true(resp != null, 'cache matched');
  assert_equals(await resp.text(), 'mod');
});
preset_match_test('match with vary not matching', new Request('http://foobar/vary-ua', { headers: { ua: 'foobar' } }), {}, async resp => {
  assert_true(resp == null, 'cache not matched');
});

preset_match_all_test('match all with ignoreSearch', 'http://foobar/with-search', { ignoreSearch: true }, async responses => {
  assert_equals(responses.length, 2, 'cache matched');
  assert_equals(await responses[0].text(), 'http://foobar/with-search?foo=bar');
  assert_equals(await responses[1].text(), 'http://foobar/with-search');
});
preset_match_all_test('match all with ignoreVary', new Request('http://foobar/vary-ua', { headers: { ua: 'foobar' } }), { ignoreVary: true }, async responses => {
  assert_equals(responses.length, 2, 'cache matched');
  assert_equals(await responses[0].text(), 'mod');
  assert_equals(await responses[1].text(), 'mod-2');
});
