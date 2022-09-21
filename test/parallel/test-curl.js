// META: flags=--expose-internals
'use strict';

const curl = loadBinding('curl');
const {
  Options,
  Codes,
  CurlMulti,
  CurlEasy,
  CURLPAUSE_CONT,
  CURL_READFUNC_PAUSE,
} = curl;
curl.init();

test(() => {
  assert_equals(typeof curl.versions, 'object');
}, 'curl versions');

promise_test(async () => {
  const multi = new CurlMulti();
  const easy = new CurlEasy();
  easy.setOpt(Options.CURLOPT_HTTPGET, 1);
  easy.setOpt(Options.CURLOPT_HTTPHEADER, [ 'foo: bar' ]);
  easy.setOpt(Options.CURLOPT_URL, 'http://localhost:30122/dump');

  const textDecoder = new TextDecoder();

  const headers = [];
  let result = '';
  let code;
  easy._onHeader = chunk => {
    headers.push(chunk);
  };
  easy._onWrite = ab => {
    result += textDecoder.decode(ab);
  };

  await new Promise(resolve => {
    easy._onDone = retcode => {
      code = retcode;
      multi.removeHandle(easy);
      multi.close();
      resolve();
    };
    multi.addHandle(easy);
  });

  assert_equals(code, Codes.CURLE_OK);
  assert_equals(headers[headers.length - 1], '\r\n');
  const json = JSON.parse(result);
  assert_equals(json.method, 'GET');
  assert_equals(json.url, '/dump');
  assert_equals(json.headers.foo, 'bar');
}, 'curl get');

promise_test(async () => {
  const textDecoder = new TextDecoder();
  const textEncoder = new TextEncoder();
  const multi = new CurlMulti();
  const easy = new CurlEasy();

  const data = textEncoder.encode('foobar');

  easy.setOpt(Options.CURLOPT_POST, 1);
  easy.setOpt(Options.CURLOPT_POSTFIELDSIZE, data.byteLength);
  easy.setOpt(Options.CURLOPT_URL, 'http://localhost:30122/dump');

  easy._onRead = (nread, ab) => {
    const target = new Uint8Array(ab);
    target.set(data);
    return data.byteLength;
  };

  const headers = [];
  let result = '';
  let code;
  easy._onHeader = chunk => {
    headers.push(chunk);
  };
  easy._onWrite = ab => {
    result += textDecoder.decode(ab);
  };

  await new Promise(resolve => {
    easy._onDone = retcode => {
      code = retcode;
      multi.removeHandle(easy);
      multi.close();
      resolve();
    };
    multi.addHandle(easy);
  });

  assert_equals(code, Codes.CURLE_OK);
  assert_equals(headers[headers.length - 1], '\r\n');
  const json = JSON.parse(result);
  assert_equals(json.method, 'POST');
  assert_equals(json.headers['content-length'], '6');
  assert_equals(json.url, '/dump');
  assert_equals(json.body, 'foobar');
}, 'curl post');

promise_test(async () => {
  const textDecoder = new TextDecoder();
  const textEncoder = new TextEncoder();
  const multi = new CurlMulti();
  const easy = new CurlEasy();

  const data = textEncoder.encode('foobar');

  easy.setOpt(Options.CURLOPT_POST, 1);
  easy.setOpt(Options.CURLOPT_URL, 'http://localhost:30122/dump');

  let sent = false;
  let pause = true;
  easy._onRead = (nread, ab) => {
    if (pause) {
      pause = false;
      setTimeout(() => {
        easy.pause(CURLPAUSE_CONT);
      }, 100);
      return CURL_READFUNC_PAUSE;
    }
    if (sent) {
      return 0;
    }
    sent = true;
    const target = new Uint8Array(ab);
    target.set(data);
    return data.byteLength;
  };

  const headers = [];
  let result = '';
  let code;
  easy._onHeader = chunk => {
    headers.push(chunk);
  };
  easy._onWrite = ab => {
    result += textDecoder.decode(ab);
  };

  await new Promise(resolve => {
    easy._onDone = retcode => {
      code = retcode;
      multi.removeHandle(easy);
      multi.close();
      resolve();
    };
    multi.addHandle(easy);
  });

  assert_equals(code, Codes.CURLE_OK);
  console.log(headers);
  assert_equals(headers[headers.length - 1], '\r\n');
  const json = JSON.parse(result);
  assert_equals(json.method, 'POST');
  assert_equals(json.headers['transfer-encoding'], 'chunked');
  assert_equals(json.url, '/dump');
  assert_equals(json.body, 'foobar');
}, 'curl post chunked encoding');
