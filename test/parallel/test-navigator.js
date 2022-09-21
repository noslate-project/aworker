'use strict';

// Adopted from wpt/html/webappapis/system-state-and-capabilities/the-navigator-object/navigator.any.js

test(function() {
  assert_equals(navigator.appCodeName, 'Mozilla');
}, 'appCodeName');

test(function() {
  assert_equals(navigator.appName, 'Netscape');
}, 'appName');

test(function() {
  assert_equals(typeof navigator.appVersion, 'string',
    'navigator.appVersion should be a string');
}, 'appVersion');

test(function() {
  assert_equals(typeof navigator.platform, 'string',
    'navigator.platform should be a string');
}, 'platform');

test(function() {
  assert_equals(navigator.product, 'Gecko');
}, 'product');

test(function() {
  assert_false('productSub' in navigator);
}, 'productSub');

test(function() {
  assert_equals(typeof navigator.userAgent, 'string',
    'navigator.userAgent should be a string');
}, 'userAgent type');

test(function() {
  assert_false('vendor' in navigator);
}, 'vendor');

test(function() {
  assert_false('vendorSub' in navigator);
}, 'vendorSub');
