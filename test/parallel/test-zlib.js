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
  const compressed = type === COMPRESS_TYPE.DEFLATE ? getDeflateCompressed() : getGzipCompressed();
  const typeName = type === COMPRESS_TYPE.DEFLATE ? 'deflate' : 'gzip';

  promise_test(async () => {
    const zip = new Zip(type, type === COMPRESS_TYPE.DEFLATE ? {} : {
    });
    const writable = zip.writable.getWriter();
    const readable = zip.readable.getReader();
    writable.write(uncompressed);
    writable.close();

    let ret = new Uint8Array(0);
    while (true) {
      const { value, done } = await readable.read();
      if (done) break;
      const after = new Uint8Array(ret.byteLength + value.byteLength);
      after.set(ret, 0);
      after.set(new Uint8Array(value), ret.byteLength);
      ret = after;
    }

    const std = new Uint8Array(compressed);
    assert_equals(ret.byteLength, std.byteLength);
    for (let i = 0; i < ret.byteLength; i++) {
      if (type === COMPRESS_TYPE.GZIP && i === 9) {
        // The 9th byte in Gzip stands for OS, and macOS is 0x13.
        // Refs: https://github.com/madler/zlib/blob/2fa463b/zutil.h#L162-L164
        if (ret[i] === 0x13) continue;
      }
      assert_equals(ret[i], std[i]);
    }
  }, `should compress ${typeName}`);
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
    const unzip = new Unzip(type);
    const writable = unzip.writable.getWriter();
    const readable = unzip.readable.getReader();
    writable.write(compressed);
    writable.close();
    let ret = await readable.read();
    assert_equals(ret.done, false);
    assert_equals(decoder.decode(ret.value), uncompressed);
    ret = await readable.read();
    assert_equals(ret.done, true);
    assert_equals(ret.value, undefined);
  }, typeName);

  promise_test(async () => {
    const unzip = new Unzip(type, { flush: FLUSH_MODE.Z_NO_FLUSH });
    const writable = unzip.writable.getWriter();
    const readable = unzip.readable.getReader();
    await writable.ready;
    await readable.ready;
    setTimeout(async () => {
      for (let i = 0; i < compressed.byteLength; i++) {
        const uint8 = new Uint8Array(compressed, i, 1);
        writable.write(uint8);
      }
      writable.close();
    }, 0);

    let ret = new Uint8Array(0);
    while (true) {
      const { done, value } = await readable.read();
      if (done) break;

      const after = new Uint8Array(ret.byteLength + value.byteLength);
      after.set(ret, 0);
      after.set(new Uint8Array(value), ret.byteLength);
      ret = after;
    }

    assert_equals(decoder.decode(ret), uncompressed);
  }, `${typeName} with chunks`);

  promise_test(async t => {
    const unzip = new Unzip(type, { flush: FLUSH_MODE.Z_NO_FLUSH });
    const writable = unzip.writable.getWriter();
    const readable = unzip.readable.getReader();
    await writable.ready;
    await readable.ready;

    setTimeout(async () => {
      for (let i = 0; i < compressed.byteLength - 10; i++) {
        const uint8 = new Uint8Array(compressed, i, 1);
        writable.write(uint8);
      }

      await promise_rejects_exactly(t, 'Uncompression is not completed yet.', (async () => {
        try {
          await writable.close();
        } catch (e) {
          throw e.message;
        }
      })());
    }, 0);

    let catched = false;
    while (true) {
      try {
        const { done } = await readable.read();
        if (done) break;
      } catch (e) {
        catched = true;
        assert_equals(e.message, 'Uncompression is not completed yet.');
        break;
      }
    }

    assert_equals(catched, true);
  }, `${typeName} close early`);
}

promise_test(async t => {
  const unzip = new Unzip(UNCOMPRESS_TYPE.UNZIP);
  const writer = unzip.writable.getWriter();
  const reader = unzip.readable.getReader();

  await promise_rejects_exactly(t, 'inflate() failed with status -3, incorrect header check.', (async () => {
    try {
      await writer.write(getUncompressed());
    } catch (e) {
      throw e.message;
    }
  })());

  await promise_rejects_exactly(t, 'inflate() failed with status -3, incorrect header check.', (async () => {
    try {
      await reader.read();
    } catch (e) {
      throw e.message;
    }
  })());

  await promise_rejects_exactly(t, 'inflate() failed with status -3, incorrect header check.', (async () => {
    try {
      await writer.write(getGzipCompressed());
    } catch (e) {
      throw e.message;
    }
  })());

  await promise_rejects_exactly(t, 'inflate() failed with status -3, incorrect header check.', (async () => {
    try {
      await reader.read();
    } catch (e) {
      throw e.message;
    }
  })());
}, 'invalid data');

promise_test(async t => {
  const unzip = new Unzip(UNCOMPRESS_TYPE.UNZIP);
  const writer = unzip.writable.getWriter();
  const reader = unzip.readable.getReader();

  await writer.write(getDeflateCompressed());
  await reader.read();
  await promise_rejects_exactly(t, 'Unzip already done.', (async () => {
    try {
      await writer.write(getUncompressed());
    } catch (e) {
      throw e.message;
    }
  })());
}, 'already finished');
