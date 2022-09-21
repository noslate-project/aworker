#pragma once

#include <google/protobuf/io/zero_copy_stream.h>
#include <functional>
#include <list>
#include <memory>
#include <vector>
#include "util.h"
#include "uv.h"

namespace aworker {
using ZeroCopyInputStream = google::protobuf::io::ZeroCopyInputStream;
using ZeroCopyOutputStream = google::protobuf::io::ZeroCopyOutputStream;

struct ZeroCopyStreamBuf {
  char* data = nullptr;
  // this is not exactly the data was initialized with. may be shrank.
  int len = 0;
  int written = 0;

  ZeroCopyStreamBuf(char* data, int len) : data(data), len(len), written(0) {}
  ~ZeroCopyStreamBuf() {
    if (data != nullptr) {
      delete[] data;
    }
  }

  AWORKER_DISALLOW_ASSIGN_COPY(ZeroCopyStreamBuf);
  ZeroCopyStreamBuf(ZeroCopyStreamBuf&& other) {
    data = other.data;
    len = other.len;
    written = other.written;
    other.data = nullptr;
    other.len = 0;
    other.written = 0;
  }
};

// Although it is named as a stream, it is not actually a streaming file reading
// pattern.
class UvZeroCopyInputFileStream : public ZeroCopyInputStream {
 public:
  using Callback =
      std::function<void(std::unique_ptr<UvZeroCopyInputFileStream>)>;
  static void Create(uv_loop_t* loop, std::string path, Callback callback);
  ~UvZeroCopyInputFileStream();

  // Ownership of this buffer remains with the stream, and the buffer remains
  // valid only until some other method of the stream is called or the stream is
  // destroyed.
  bool Next(const void** data, int* size) override;
  void BackUp(int count) override;
  bool Skip(int count) override;
  int64_t ByteCount() const override;

 private:
  static void ReadCb(uv_fs_t* req);
  void ReadNext();
  UvZeroCopyInputFileStream(uv_loop_t* loop, int fd, Callback callback);

  uv_loop_t* loop_;
  uv_file fd_ = -1;
  uv_fs_t req_;
  std::vector<ZeroCopyStreamBuf> bufs_;
  int64_t fs_read_offset_ = 0;

  size_t read_chunk_idx_ = 0;
  int last_chunk_read_offset_ = 0;
  Callback callback_;
};

// Although it is named as a stream, it is not actually a streaming file writing
// pattern.
class UvZeroCopyOutputFileStream : public ZeroCopyOutputStream {
 public:
  static void DisposeAndDelete(UvZeroCopyOutputFileStream* stream);
  using UvZeroCopyOutputFileStreamPtr =
      DeleteFnPtr<UvZeroCopyOutputFileStream, DisposeAndDelete>;
  using Callback = std::function<void(UvZeroCopyOutputFileStream*)>;
  static UvZeroCopyOutputFileStreamPtr Create(uv_loop_t* loop,
                                              std::string path,
                                              Callback on_end);

  // Ownership of this buffer remains with the stream, and the buffer remains
  // valid only until some other method of the stream is called or the stream is
  // destroyed.
  bool Next(void** data, int* size) override;
  void BackUp(int count) override;
  int64_t ByteCount() const override;

 private:
  static void WriteCb(uv_fs_t* req);
  static void IdleCb(uv_idle_t* handle);
  void WriteNext();
  UvZeroCopyOutputFileStream(uv_loop_t* loop, int fd, Callback on_end);
  ~UvZeroCopyOutputFileStream();

  void set_writing_for_write(bool);

  uv_loop_t* loop_;
  uv_file fd_ = -1;
  uv_fs_t req_;
  uv_idle_t* idle_;
  int64_t written_count_ = 0;
  bool waiting_for_write_ = false;
  bool waiting_for_dispose_ = false;

  std::list<ZeroCopyStreamBuf> queue_;
  Callback on_end_;
};

}  // namespace aworker
