#include "zero_copy_file_stream.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "aworker_logger.h"
#include "debug_utils.h"
#include "util.h"

#define MAX_PIPE_PAGE 65536

namespace aworker {
void UvZeroCopyInputFileStream::Create(uv_loop_t* loop,
                                       std::string path,
                                       Callback callback) {
  uv_fs_t open_req;
  // TODO(chengzhong.wcz): async open;
  const uv_file fd =
      uv_fs_open(loop, &open_req, path.c_str(), O_RDONLY, 0, nullptr);
  uv_fs_req_cleanup(&open_req);
  if (fd < 0) {
    DLOG("UvZeroCopyInputFileStream::Create(%s): %s",
         path.c_str(),
         strerror(errno));
    callback(nullptr);
    return;
  }
  auto ptr = new UvZeroCopyInputFileStream(loop, fd, callback);
  ptr->ReadNext();
}

void UvZeroCopyInputFileStream::ReadCb(uv_fs_t* req) {
  UvZeroCopyInputFileStream* stream =
      ContainerOf(&UvZeroCopyInputFileStream::req_, req);

  ssize_t nread = req->result;
  uv_fs_req_cleanup(&stream->req_);
  if (nread < 0) {
    // TODO(chengzhong.wcz): proper error handling;
    ELOG("Read error: %s", uv_strerror(nread));
    return;
  }

  stream->fs_read_offset_ += nread;
  if (nread == 0) {
    stream->bufs_.pop_back();

    uv_fs_t close_req;
    // TODO(chengzhong.wcz): async close;
    uv_fs_close(stream->loop_, &close_req, stream->fd_, nullptr);
    uv_fs_req_cleanup(&close_req);
    // EOF
    stream->callback_(std::unique_ptr<UvZeroCopyInputFileStream>(stream));
    return;
  }
  stream->bufs_.back().len = nread;
  stream->ReadNext();
}

void UvZeroCopyInputFileStream::ReadNext() {
  char* data = new char[MAX_PIPE_PAGE];
  bufs_.emplace_back(data, MAX_PIPE_PAGE);

  uv_buf_t buf = uv_buf_init(data, MAX_PIPE_PAGE);
  uv_fs_read(loop_, &req_, fd_, &buf, 1, fs_read_offset_, ReadCb);
}

UvZeroCopyInputFileStream::UvZeroCopyInputFileStream(uv_loop_t* loop,
                                                     uv_file fd,
                                                     Callback callback)
    : ZeroCopyInputStream(), loop_(loop), fd_(fd), callback_(callback) {}

UvZeroCopyInputFileStream::~UvZeroCopyInputFileStream() {}

bool UvZeroCopyInputFileStream::Next(const void** data, int* size) {
  CHECK_NE(data, nullptr);
  CHECK_NE(size, nullptr);
  if (read_chunk_idx_ >= bufs_.size()) {
    return false;
  }
  auto& chunk = bufs_[read_chunk_idx_];
  *data = chunk.data + last_chunk_read_offset_;
  *size = chunk.len - last_chunk_read_offset_;
  read_chunk_idx_++;
  last_chunk_read_offset_ = 0;
  return true;
}

void UvZeroCopyInputFileStream::BackUp(int count) {
  read_chunk_idx_--;
  auto& chunk = bufs_[read_chunk_idx_];
  last_chunk_read_offset_ = chunk.len - count;
}

bool UvZeroCopyInputFileStream::Skip(int count) {
  int rem = count;
  while (rem >= 0 && read_chunk_idx_ < bufs_.size()) {
    auto& chunk = bufs_[read_chunk_idx_];
    rem -= chunk.len - last_chunk_read_offset_;
    read_chunk_idx_++;
  }
  if (read_chunk_idx_ == bufs_.size()) {
    return false;
  }
  return true;
}

int64_t UvZeroCopyInputFileStream::ByteCount() const {
  int64_t count = 0;
  for (size_t idx = 0; idx < read_chunk_idx_; idx++) {
    auto& chunk = bufs_[read_chunk_idx_];
    count += chunk.len;
  }
  if (read_chunk_idx_ <= bufs_.size() && last_chunk_read_offset_ > 0) {
    count += last_chunk_read_offset_;
  }
  return count;
}

void UvZeroCopyOutputFileStream::DisposeAndDelete(
    UvZeroCopyOutputFileStream* stream) {
  stream->waiting_for_dispose_ = true;
  if (!stream->waiting_for_write_) {
    uv_fs_t close_req;
    // TODO(chengzhong.wcz): async close;
    uv_fs_close(stream->loop_, &close_req, stream->fd_, nullptr);
    uv_fs_req_cleanup(&close_req);

    stream->on_end_(stream);
    delete stream;
  }
}

UvZeroCopyOutputFileStream::UvZeroCopyOutputFileStreamPtr
UvZeroCopyOutputFileStream::Create(uv_loop_t* loop,
                                   std::string path,
                                   Callback on_end) {
  unlink(path.c_str());

  uv_fs_t open_req;
  // TODO(chengzhong.wcz): async open;
  const uv_file fd = uv_fs_open(
      loop, &open_req, path.c_str(), O_WRONLY | O_CREAT, 0644, nullptr);
  uv_fs_req_cleanup(&open_req);
  if (fd < 0) {
    ELOG("Create out %s: %s", path.c_str(), strerror(errno));
    return nullptr;
  }
  auto ptr = new UvZeroCopyOutputFileStream(loop, fd, on_end);
  return UvZeroCopyOutputFileStreamPtr(ptr);
}

void UvZeroCopyOutputFileStream::WriteCb(uv_fs_t* req) {
  UvZeroCopyOutputFileStream* stream =
      ContainerOf(&UvZeroCopyOutputFileStream::req_, req);

  ssize_t nwrite = req->result;
  uv_fs_req_cleanup(req);
  if (nwrite < 0) {
    // TODO(chengzhong.wcz): proper error handling;
    ELOG("Write error: %s", uv_strerror(nwrite));
    return;
  }

  stream->written_count_ += nwrite;
  auto& buf = stream->queue_.front();
  buf.written += nwrite;
  if (buf.written == buf.len) {
    stream->queue_.pop_front();
  }

  if (stream->queue_.size() > 0) {
    stream->WriteNext();
    return;
  }

  stream->set_writing_for_write(false);
  if (stream->waiting_for_dispose_) {
    uv_fs_t close_req;
    // TODO(chengzhong.wcz): async close;
    uv_fs_close(stream->loop_, &close_req, stream->fd_, nullptr);
    uv_fs_req_cleanup(&close_req);
    stream->on_end_(stream);
    delete stream;
  }
}

void UvZeroCopyOutputFileStream::WriteNext() {
  auto& req = queue_.front();
  uv_buf_t buf = uv_buf_init(req.data + req.written, req.len - req.written);

  uv_fs_write(loop_, &req_, fd_, &buf, 1, written_count_, WriteCb);
}

UvZeroCopyOutputFileStream::UvZeroCopyOutputFileStream(uv_loop_t* loop,
                                                       int fd,
                                                       Callback on_end)
    : ZeroCopyOutputStream(),
      loop_(loop),
      fd_(fd),
      idle_(new uv_idle_t),
      on_end_(on_end) {
  uv_idle_init(loop, idle_);
  idle_->data = this;
}

UvZeroCopyOutputFileStream::~UvZeroCopyOutputFileStream() {
  uv_fs_req_cleanup(&req_);
  uv_close(reinterpret_cast<uv_handle_t*>(idle_), [](uv_handle_t* handle) {
    uv_idle_t* idle = reinterpret_cast<uv_idle_t*>(handle);
    delete idle;
  });
}

bool UvZeroCopyOutputFileStream::Next(void** data, int* size) {
  CHECK_NE(data, nullptr);
  CHECK_NE(size, nullptr);
  CHECK_EQ(waiting_for_dispose_, false);

  char* buf = new char[MAX_PIPE_PAGE];
  queue_.emplace_back(buf, MAX_PIPE_PAGE);
  *data = buf;
  *size = MAX_PIPE_PAGE;

  set_writing_for_write(true);
  return true;
}

void UvZeroCopyOutputFileStream::BackUp(int count) {
  auto& last = queue_.back();
  CHECK_EQ(last.written, 0);
  last.len -= count;
}

int64_t UvZeroCopyOutputFileStream::ByteCount() const {
  return written_count_;
}

void UvZeroCopyOutputFileStream::IdleCb(uv_idle_t* handle) {
  UvZeroCopyOutputFileStream* stream =
      static_cast<UvZeroCopyOutputFileStream*>(handle->data);
  stream->WriteNext();
  uv_idle_stop(stream->idle_);
}

void UvZeroCopyOutputFileStream::set_writing_for_write(bool it) {
  if (waiting_for_write_ == it) {
    return;
  }
  waiting_for_write_ = it;
  if (waiting_for_write_) {
    // delay the write for possible BackUp in synchronous sequence calls.
    uv_idle_start(idle_, IdleCb);
  }
}

}  // namespace aworker
