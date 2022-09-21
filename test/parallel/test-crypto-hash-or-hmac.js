'use strict';

const { Hash, Hmac, randomBytes } = aworker.crypto;

const algos = [ 'sha256', 'sha512', 'sha1', 'md5' ];

let std = [
  '2955c58da69a5f3016b5ab5975845a269013ca527d5924e5b6c0b6f97888e6eb',
  '6c30b78e38f1374106a91b42ca749a267a8b364ca18fc1e0a67e12f040528c3d8333b7f0afd60fc3d8999fabdd2000210c6143bc1a6fbdfd31866c7c7ca8eea2',
  '4d91c59e11c024b5a52914520faaacaff4b6c715',
  '566c1f9ca64e27e4b7bad0ec19aecac5',
];

for (let i = 0; i < algos.length; i++) {
  (function(i) {
    test(() => {
      const hash = new Hash(algos[i]);

      hash.update('hello world!');
      hash.update('Node 架构');

      // std from Node.js
      assert_equals(hash.final().digest('hex'), std[i]);
    }, `hash ${algos[i]} -> hex`);
  })(i);
}

for (let i = 0; i < algos.length; i++) {
  (function(i) {
    test(() => {
      const hash = new Hash(algos[i]);

      hash.update('hello world!');
      hash.update('Node 架构');

      // std from Node.js
      const bin = hash.final().digest('binary');
      const uint8Array = new Uint8Array(bin);
      for (let j = 0; j < uint8Array.length; j++) {
        let hex = uint8Array[j].toString(16);
        if (hex.length === 1) hex = `0${hex}`;
        assert_equals(hex, std[i].substr(j * 2, 2));
      }
    }, `hash ${algos[i]} -> binary`);
  })(i);
}

std = [
  'KVXFjaaaXzAWtatZdYRaJpATylJ9WSTltsC2+XiI5us=',
  'bDC3jjjxN0EGqRtCynSaJnqLNkyhj8Hgpn4S8EBSjD2DM7fwr9YPw9iZn6vdIAAhDGFDvBpvvf0xhmx8fKjuog==',
  'TZHFnhHAJLWlKRRSD6qsr/S2xxU=',
  'VmwfnKZOJ+S3utDsGa7KxQ==',
];

for (let i = 0; i < algos.length; i++) {
  (function(i) {
    test(() => {
      const hash = new Hash(algos[i]);

      hash.update('hello world!');
      hash.update('Node 架构');

      // std from Node.js
      assert_equals(hash.final().digest('base64'), std[i]);
    }, `hash ${algos[i]} -> base64`);
  })(i);
}

std = [
  '3914e6e0405cb7a0734675c93827a808b2d7c6e93418986b259226f05ced48a5',
  '4cc63db8ae1dc583a1e8f4d4bb1785ea6c9dba24ea1054fd48c2a1c6e6b1a4a468bea1cc00f2531889b59e29703bcb776bec69387ff01455f9c95b912f6d77fb',
  'b80ad3af1acc37a8520b9dfc42eaa1b059566dbe',
  '033c8d1883f278bcb877569d18c1ff41',
];

for (let i = 0; i < algos.length; i++) {
  (function(i) {
    test(() => {
      const hash = new Hmac(algos[i], '🤣');

      hash.update('hello world!');
      hash.update('Node 架构');

      // std from Node.js
      assert_equals(hash.final().digest('hex'), std[i]);
    }, `hmac ${algos[i]} -> hex`);
  })(i);
}

std = [
  'b529c83da1a445944b84c1393a2b6324b91e00eea4688d83c51c9149aa2b848a',
  'd8792ec7613e2ee64c528fec6175b451598ac007ce6e743f2a0de0b983d06eb1a09fd56f4e8a3ddaaef4cc3757faf2b0b4b0b0765ee572848881182570aa3143',
  'ded9417bf1e5a3f1a40260fac98f082d9c5146fc',
  '721280f5bb1a0a29a25879a296d26bcb',
];

for (let i = 0; i < algos.length; i++) {
  (function(i) {
    test(() => {
      const hash = new Hmac(algos[i], new Uint8Array([ 1, 2, 3, 4 ]));

      hash.update('hello world!');
      hash.update('Node 架构');

      // std from Node.js
      const bin = hash.final().digest('binary');
      const uint8Array = new Uint8Array(bin);
      for (let j = 0; j < uint8Array.length; j++) {
        let hex = uint8Array[j].toString(16);
        if (hex.length === 1) hex = `0${hex}`;
        assert_equals(hex, std[i].substr(j * 2, 2));
      }
    }, `hmac ${algos[i]} -> binary`);
  })(i);
}

std = [
  'tSnIPaGkRZRLhME5OitjJLkeAO6kaI2DxRyRSaorhIo=',
  '2Hkux2E+LuZMUo/sYXW0UVmKwAfObnQ/Kg3guYPQbrGgn9VvToo92q70zDdX+vKwtLCwdl7lcoSIgRglcKoxQw==',
  '3tlBe/Hlo/GkAmD6yY8ILZxRRvw=',
  'chKA9bsaCimiWHmiltJryw==',
];

for (let i = 0; i < algos.length; i++) {
  (function(i) {
    test(() => {
      const uint8Array = new Uint8Array([ 1, 2, 3, 4 ]);
      const hash = new Hmac(algos[i], uint8Array.buffer);

      hash.update('hello world!');
      hash.update('Node 架构');

      // std from Node.js
      assert_equals(hash.final().digest('base64'), std[i]);
    }, `hmac ${algos[i]} -> base64`);
  })(i);
}

test(() => {
  for (let i = 0; i < 10; i++) {
    const size = Math.floor(Math.random() * 100);
    const bytes = randomBytes(size);

    assert_true(bytes instanceof Uint8Array);
    assert_equals(bytes.byteLength, size);
  }

  const bytes = randomBytes(0);
  assert_true(bytes instanceof Uint8Array);
  assert_equals(bytes.byteLength, 0);
}, 'randomBytes');
