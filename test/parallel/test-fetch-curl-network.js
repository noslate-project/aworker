// META: flags=--experimental-curl-fetch --expose-internals
'use strict';

const { Codes } = loadBinding('curl');

promise_test(async t => {
  const p = fetch('http://unreachable-host/echo', { method: 'GET' });
  promise_rejects_js(t, TypeError, p);

  try {
    await p;
    assert_unreached('unreachable');
  } catch (e) {
    assert_regexp_match(String(e), /Couldn't resolve host name/);
    assert_equals(e.code, Codes.CURLE_COULDNT_RESOLVE_HOST);
  }
}, 'fetch GET unresolvable host name');

promise_test(async function(t) {
  const p = fetch('http://localhost:30122/abort', { method: 'GET' });
  promise_rejects_js(t, TypeError, p);
  try {
    await p;
    assert_unreached('unreachable');
  } catch (e) {
    assert_regexp_match(String(e), /Server returned nothing/);
    assert_equals(e.code, Codes.CURLE_GOT_NOTHING);
  }
}, 'fetch GET server abort response');

promise_test(async function(t) {
  const p = fetch('http://localhost:54321', { method: 'GET' });
  promise_rejects_js(t, TypeError, p);
  try {
    await p;
    assert_unreached('unreachable');
  } catch (e) {
    assert_regexp_match(String(e), /Couldn't connect to server/);
    assert_equals(e.code, Codes.CURLE_COULDNT_CONNECT);
  }
}, 'fetch GET server unreachable');
