#include "unzip_task.h"
#include "debug_utils.h"
#include "zlib_wrapper.h"

namespace aworker {
namespace zlib {

/**
 * Reference for UnzipTask::Tick():
 *  > https://zlib.net/zlib_how.html
 *
 * int inf(FILE *source) {
 *   int ret;
 *   unsigned have;
 *   z_stream strm;
 *   unsigned char in[1];
 *   unsigned char out[4097];
 *
 *   // allocate inflate state
 *   strm.zalloc = Z_NULL;
 *   strm.zfree = Z_NULL;
 *   strm.opaque = Z_NULL;
 *   strm.avail_in = 0;
 *   strm.next_in = Z_NULL;
 *   ret = inflateInit2(&strm, MAX_WBITS + 16);
 *   if (ret != Z_OK)
 *       return ret;
 *
 *   // decompress until deflate stream ends or end of file
 *   do {
 *     strm.avail_in = fread(in, 1, 1, source);
 *     if (ferror(source)) {
 *         (void)inflateEnd(&strm);
 *         return Z_ERRNO;
 *     }
 *     if (strm.avail_in == 0)
 *         break;
 *     strm.next_in = in;
 *
 *     // run inflate() on input until output buffer not full
 *     do {
 *       strm.avail_out = 4096;
 *       strm.next_out = out;
 *
 *       ret = inflate(&strm, Z_NO_FLUSH);
 *       CHECK(ret != Z_STREAM_ERROR);  // state not clobbered
 *       switch (ret) {
 *       case Z_NEED_DICT:
 *           ret = Z_DATA_ERROR;     // and fall through
 *       case Z_DATA_ERROR:
 *       case Z_MEM_ERROR:
 *           (void)inflateEnd(&strm);
 *           return ret;
 *       }
 *
 *       have = 4096 - strm.avail_out;
 *       out[have] = 0;
 *     } while (strm.avail_out == 0);
 *     // done when inflate() says it's done
 *   } while (ret != Z_STREAM_END);
 *
 *   // clean up and return
 *   (void)inflateEnd(&strm);
 *   return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
 * }
 */
void UnzipTask::OnWorkTick() {
  // if error occurred already, or finished already, do not tick any more.
  if (_parent->finished()) {
    Fail("Uncompression already done.");
    return;
  } else if (_parent->errored()) {
    Fail("Uncompression already failed.");
    return;
  }

  if (!_prepared) Prepare();

  ZlibStreamPtr stream = _parent->stream();
  ResizableBuffer* chunk_buffer = _parent->chunk_buffer();

  stream->avail_out = chunk_buffer->byte_length();
  stream->next_out = static_cast<unsigned char*>(chunk_buffer->buffer());

  per_process::Debug(DebugCategory::ZLIB,
                     "[trace] up to do unzip task with avail_in: %d, "
                     "avail_out: %d, flush: %d\n",
                     stream->avail_in,
                     stream->avail_out,
                     _flush_mode);

  int status = inflate(stream, _flush_mode);
  per_process::Debug(DebugCategory::ZLIB,
                     "[call] inflate(stream, _flush_mode: %d): %d\n",
                     _flush_mode,
                     status);

  char error_msg[128] = {0};
  switch (status) {
    case Z_STREAM_ERROR:
      snprintf(error_msg, sizeof(error_msg), "unexpected end of file.");
      break;

    case Z_NEED_DICT:
      status = Z_DATA_ERROR;  // fall through

    case Z_DATA_ERROR:
    case Z_MEM_ERROR: {
      snprintf(error_msg,
               sizeof(error_msg),
               "inflate() failed with status %i, %s.",
               status,
               stream->msg);
      break;
    }
    default: {
      break;
    }
  }

  per_process::Debug(DebugCategory::ZLIB,
                     "[trace] unzip task done with status: %d, avail_in: %d, "
                     "avail_out: %d, error: %s\n",
                     status,
                     stream->avail_in,
                     stream->avail_out,
                     error_msg);

  if (*error_msg != 0) {
    Fail(error_msg);
    return;
  }

  // The output of inflate() is handled identically to that of deflate().
  ConcatChunkToResult(stream, *chunk_buffer);

  // When inflate() reports that it has reached the end of the input zlib
  // stream, has completed the decompression and integrity check, and has
  // provided all of the output. This is indicated by the inflate() return value
  // Z_STREAM_END.
  if (status == Z_STREAM_END) {
    Done(true /** all done */);
    return;
  }

  // When inflate() has no more output as indicated by not filling the output
  // buffer. In this case, we cannot assert that strm.avail_in will be zero,
  // since the deflate stream may end before the file does.
  if (stream->avail_out != 0) {
    Done(false /** not all done */);
    return;
  }

  // Wait for next task tick
  CHECK_EQ(status, Z_OK);

  per_process::Debug(DebugCategory::ZLIB,
                     "[trace] unzip task wait for next tick\n");
}

}  // namespace zlib
}  // namespace aworker
