'use strict';

function bURL(url, base) {
  return new URL(url, base || 'about:blank');
}

const skipped = [
  [ 'http://0177.0.0.0189', 'https://github.com/web-platform-tests/wpt/commit/a85b7d62bd2788e36e6e83c043f86b3ba1e6a9ec' ],
  [ 'http://256.256.256.256.256', 'https://github.com/web-platform-tests/wpt/commit/a85b7d62bd2788e36e6e83c043f86b3ba1e6a9ec' ],
  [ 'http://0..0x300/', 'https://github.com/web-platform-tests/wpt/commit/a85b7d62bd2788e36e6e83c043f86b3ba1e6a9ec' ],
  [ 'http://example.com/\uD800\uD801\uDFFE\uDFFF\uFDD0\uFDCF\uFDEF\uFDF0\uFFFE\uFFFF?\uD800\uD801\uDFFE\uDFFF\uFDD0\uFDCF\uFDEF\uFDF0\uFFFE\uFFFF', '' ],
  [ "http://\u001F!\"$&'()*+,-.;=_`{|}~/", 'https://github.com/web-platform-tests/wpt/commit/53a00b35ce2808bbbef9699ac4b6f0d5da46009a' ],
  [ "sc://\u001F!\"$&'()*+,-.;=_`{|}~/", 'https://github.com/web-platform-tests/wpt/commit/53a00b35ce2808bbbef9699ac4b6f0d5da46009a' ],
];

function runURLTests(urltests) {
  for (let i = 0, l = urltests.length; i < l; i++) {
    const expected = urltests[i];
    if (typeof expected === 'string') continue; // skip comments

    if (skipped.findIndex(it => it[0] === expected.input) >= 0) {
      continue; // skip invalid cases
    }

    test(function() {
      if (expected.failure) {
        assert_throws_js(TypeError, function() {
          bURL(expected.input, expected.base);
        });
        return;
      }

      const url = bURL(expected.input, expected.base);
      assert_equals(url.href, expected.href, 'href');
      assert_equals(url.protocol, expected.protocol, 'protocol');
      assert_equals(url.username, expected.username, 'username');
      assert_equals(url.password, expected.password, 'password');
      assert_equals(url.host, expected.host, 'host');
      assert_equals(url.hostname, expected.hostname, 'hostname');
      assert_equals(url.port, expected.port, 'port');
      assert_equals(url.pathname, expected.pathname, 'pathname');
      assert_equals(url.search, expected.search, 'search');
      if ('searchParams' in expected) {
        assert_true('searchParams' in url);
        assert_equals(url.searchParams.toString(), expected.searchParams, 'searchParams');
      }
      assert_equals(url.hash, expected.hash, 'hash');
    }, 'Parsing: <' + expected.input + '> against <' + expected.base + '>');
  }
}

promise_test(() => fetch('url/resources/urltestdata.json').then(res => res.json()).then(runURLTests), 'Loading data…');
