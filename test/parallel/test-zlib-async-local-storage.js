// META: flags=--expose-internals
'use strict';

const { zlib } = aworker;
const path = load('path');
const file = load('file');
const dirname = path.dirname(location.pathname);

const decoder = new TextDecoder('utf8');

const { FLUSH_MODE, Unzip, Zip, UNCOMPRESS_TYPE, COMPRESS_TYPE } = zlib;

function getUncompressed() {
  return file.readFile(path.join(dirname, '../fixtures/zlib/1_uncompressed.txt'), 'utf8');
}

function getGzipCompressed() {
  return file.readFile(path.join(dirname, '../fixtures/zlib/1_compressed_gzip.txt'));
}

function getDeflateCompressed() {
  return file.readFile(path.join(dirname, '../fixtures/zlib/1_compressed_deflate.txt'));
}

for (const type of [ COMPRESS_TYPE.DEFLATE, COMPRESS_TYPE.GZIP ]) {
  const uncompressed = getUncompressed();
  const typeName = type === COMPRESS_TYPE.DEFLATE ? 'deflate' : 'gzip';

  promise_test(async () => {
    const storage = new aworker.AsyncLocalStorage();

    await storage.run(1, async () => {
      assert_equals(storage.getStore(), 1);
      const zip = new Zip(type);
      const writable = zip.writable.getWriter();
      const readable = zip.readable.getReader();
      writable.write(uncompressed);
      writable.close();

      while (true) {
        const { done } = await readable.read();
        assert_equals(storage.getStore(), 1);
        if (done) break;
      }
    });

    assert_equals(storage.getStore(), undefined);
  }, `compressing ${typeName} should propagate async local storage`);
}

for (const type of [ UNCOMPRESS_TYPE.UNZIP, UNCOMPRESS_TYPE.GUNZIP, UNCOMPRESS_TYPE.INFLATE ]) {
  const uncompressed = getUncompressed();
  let compressed;
  let typeName = '';
  switch (type) {
    case UNCOMPRESS_TYPE.UNZIP: {
      typeName = 'Unzip';
    }

    case UNCOMPRESS_TYPE.GUNZIP: { // eslint-disable-line
      if (!typeName) typeName = 'Gunzip';
      compressed = getGzipCompressed();
      break;
    }

    default: {
      typeName = 'Inflate';
      compressed = getDeflateCompressed();
    }
  }

  promise_test(async () => {
    const storage = new aworker.AsyncLocalStorage();

    await storage.run(1, async () => {
      assert_equals(storage.getStore(), 1);
      const unzip = new Unzip(type);
      const writable = unzip.writable.getWriter();
      const readable = unzip.readable.getReader();
      writable.write(compressed);
      writable.close();

      let ret = await readable.read();
      assert_equals(storage.getStore(), 1);
      assert_equals(ret.done, false);
      assert_equals(decoder.decode(ret.value), uncompressed);

      ret = await readable.read();
      assert_equals(storage.getStore(), 1);
      assert_equals(ret.done, true);
      assert_equals(ret.value, undefined);
    });

    assert_equals(storage.getStore(), undefined);
  }, `${typeName} should propagate async local storage`);

  promise_test(async () => {
    const storage = new aworker.AsyncLocalStorage();

    await storage.run(1, async () => {
      assert_equals(storage.getStore(), 1);

      const unzip = new Unzip(type, { flush: FLUSH_MODE.Z_NO_FLUSH });
      const writable = unzip.writable.getWriter();
      const readable = unzip.readable.getReader();
      await writable.ready;
      assert_equals(storage.getStore(), 1);

      await readable.ready;
      assert_equals(storage.getStore(), 1);

      setTimeout(async () => {
        assert_equals(storage.getStore(), 1);
        for (let i = 0; i < compressed.byteLength; i++) {
          const uint8 = new Uint8Array(compressed, i, 1);
          writable.write(uint8);
        }
        writable.close();
      }, 0);

      while (true) {
        const { done } = await readable.read();
        assert_equals(storage.getStore(), 1);
        if (done) break;
      }
    });

    assert_equals(storage.getStore(), undefined);
  }, `${typeName} with chunks should propagate async local storage`);
}
