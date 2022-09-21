#include "zip_task.h"
#include "debug_utils.h"
#include "zlib_wrapper.h"

namespace aworker {
namespace zlib {

/**
 * Reference for ZipTask::Tick():
 *  > https://zlib.net/zlib_how.html
 *
 * int def(FILE *source, FILE *dest, int level)
 * {
 *     int ret, flush;
 *     unsigned have;
 *     z_stream strm;
 *     unsigned char in[CHUNK];
 *     unsigned char out[CHUNK];
 *
 *     // allocate deflate state
 *     strm.zalloc = Z_NULL;
 *     strm.zfree = Z_NULL;
 *     strm.opaque = Z_NULL;
 *     ret = deflateInit(&strm, level);
 *     if (ret != Z_OK)
 *         return ret;
 *
 *     // compress until end of file
 *     do {
 *         strm.avail_in = fread(in, 1, CHUNK, source);
 *         if (ferror(source)) {
 *             (void)deflateEnd(&strm);
 *             return Z_ERRNO;
 *         }
 *         flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
 *         strm.next_in = in;
 *         // run deflate() on input until output buffer not full, finish
 *         // compression if all of source has been read in
 *         do {
 *             strm.avail_out = CHUNK;
 *             strm.next_out = out;
 *             ret = deflate(&strm, flush);    // no bad return value
 *             assert(ret != Z_STREAM_ERROR);  // state not clobbered
 *             have = CHUNK - strm.avail_out;
 *             if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
 *                 (void)deflateEnd(&strm);
 *                 return Z_ERRNO;
 *             }
 *         } while (strm.avail_out == 0);
 *         assert(strm.avail_in == 0);     // all input will be used
 *         // done when last data in file processed
 *     } while (flush != Z_FINISH);
 *     assert(ret == Z_STREAM_END);        // stream will be complete
 *     // clean up and return
 *     (void)deflateEnd(&strm);
 *     return Z_OK;
 * }
 */
void ZipTask::OnWorkTick() {
  // if error occurred already, or finished already, do not tick any more.
  if (_parent->finished()) {
    Fail("Compression already done.");
    return;
  } else if (_parent->errored()) {
    Fail("Compression already failed.");
    return;
  }

  if (!_prepared) Prepare();

  ZlibStreamPtr stream = _parent->stream();
  ResizableBuffer* chunk_buffer = _parent->chunk_buffer();

  stream->avail_out = chunk_buffer->byte_length();
  stream->next_out = static_cast<unsigned char*>(chunk_buffer->buffer());

  per_process::Debug(
      DebugCategory::ZLIB,
      "[trace] up to do zip task with avail_in: %d, avail_out: %d, flush: %d\n",
      stream->avail_in,
      stream->avail_out,
      _flush_mode);

  int status = deflate(stream, _flush_mode);
  per_process::Debug(DebugCategory::ZLIB,
                     "[call] deflate(stream, _flush_mode: %d): %d\n",
                     _flush_mode,
                     status);

  char error_msg[128] = {0};
  switch (status) {
    case Z_STREAM_ERROR:
      snprintf(error_msg, sizeof(error_msg), "unexpected end of file.");
      break;

    case Z_BUF_ERROR:
    case Z_DATA_ERROR:
    case Z_MEM_ERROR: {
      snprintf(error_msg,
               sizeof(error_msg),
               "deflate() failed with status %i, %s.",
               status,
               stream->msg);
      break;
    }

    default: {
      break;
    }
  }

  per_process::Debug(DebugCategory::ZLIB,
                     "[trace] zip task done with status: %d, avail_in: %d, "
                     "avail_out: %d, error: %s\n",
                     status,
                     stream->avail_in,
                     stream->avail_out,
                     error_msg);

  if (*error_msg != 0) {
    Fail(error_msg);
    return;
  }

  ConcatChunkToResult(stream, *chunk_buffer);

  if (status == Z_STREAM_END && _flush_mode == Z_FINISH) {
    Done(true /** all done */);
    return;
  }

  if (stream->avail_out != 0) {
    Done(false /** not all done */);
    return;
  }

  // Wait for next task tick
  CHECK_EQ(status, Z_OK);
  per_process::Debug(DebugCategory::ZLIB,
                     "[trace] zip task wait for next tick\n");
}

}  // namespace zlib
}  // namespace aworker
