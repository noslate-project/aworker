'use strict';

test(() => {
  assert_equals(aworker.idna.toASCII('好的'), 'xn--5us888d');
  assert_equals(aworker.idna.toASCII('OK'), 'ok');
}, 'idna.toASCII');
