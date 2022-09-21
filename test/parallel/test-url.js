'use strict';

test(() => {
  const url = new URL('http://1.2.3.4');
  assert_equals(url.toString(), 'http://1.2.3.4/');
}, 'IPv4 parse');

test(() => {
  const url = new URL('http://www.example.com');
  url.host = '1.2.3.4';
  assert_equals(url.toString(), 'http://1.2.3.4/');
}, 'IPv4 host setter');
