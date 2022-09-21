#include "ipc/ipc_socket_uv.h"
#include "aworker_logger.h"
#include "util.h"

namespace aworker {
namespace ipc {

/**
 * MARK: - helper types
 */

static void allocate_buffer(uv_handle_t* stream, size_t len, uv_buf_t* buf) {
  *buf = uv_buf_init(new char[len], len);
}

class ClientConnectCallback {
 public:
  explicit ClientConnectCallback(
      std::function<void(UvSocketHolder::Pointer)> onconnect)
      : onconnect_(onconnect) {}
  void operator()(UvSocketHolder::Pointer ptr) const { onconnect_(move(ptr)); }

 private:
  std::function<void(UvSocketHolder::Pointer)> onconnect_;
};

class WriteReqData {
 public:
  WriteReqData(UvSocketHolder* holder, char* data, size_t len)
      : _holder(holder), _data(data), _len(len) {}
  ~WriteReqData() { delete[] _data; }
  UvSocketHolder* _holder;
  char* _data;
  size_t _len;
};

/**
 * MARK: - static members
 */

UvSocketHolder::Pointer UvSocketHolder::Accept(
    uv_stream_t* server, std::shared_ptr<SocketDelegate> delegate) {
  auto loop = std::make_shared<UvLoop>(server->loop);
  UvSocketHolder* result = new UvSocketHolder(loop, std::move(delegate));
  uv_stream_t* pipe = reinterpret_cast<uv_stream_t*>(&result->pipe_);
  int err = uv_pipe_init(server->loop, &result->pipe_, false);
  if (err == 0) {
    err = uv_accept(server, pipe);
  }
  if (err == 0) {
    err = uv_read_start(pipe, allocate_buffer, OnDataReceivedCb);
  }
  if (err == 0) {
    return UvSocketHolder::Pointer(result);
  } else {
    delete result;
    return nullptr;
  }
}

void UvSocketHolder::Connect(
    std::shared_ptr<UvLoop> loop,
    std::string server_address,
    std::shared_ptr<SocketDelegate> delegate,
    std::function<void(UvSocketHolder::Pointer)> onconnect) {
  UvSocketHolder* result = new UvSocketHolder(loop, std::move(delegate));
  int err = uv_pipe_init(*loop, &result->pipe_, false);
  if (err != 0) {
    loop->SetImmediate([loop, onconnect](Immediate* immediate) {
      loop->ClearImmediate(immediate);
      onconnect(nullptr);
    });
    delete result;
    return;
  }
  uv_connect_t* req = new uv_connect_t();
  req->data = new ClientConnectCallback(onconnect);
  uv_pipe_connect(req, &result->pipe_, server_address.c_str(), OnConnectCb);
}

void UvSocketHolder::OnConnectCb(uv_connect_t* req, int status) {
  ClientConnectCallback* callback =
      reinterpret_cast<ClientConnectCallback*>(req->data);
  uv_stream_t* handle = req->handle;
  UvSocketHolder* holder = UvSocketHolder::From(handle);
  int err = uv_read_start(handle, allocate_buffer, OnDataReceivedCb);
  if (err == 0) {
    (*callback)(UvSocketHolder::Pointer(holder));
  } else {
    delete holder;
    (*callback)(nullptr);
  }
  delete req;
  delete callback;
}

void UvSocketHolder::OnDataReceivedCb(uv_stream_t* pipe,
                                      ssize_t nread,
                                      const uv_buf_t* buf) {
  DLOG("read %zd bytes", nread);
#if DUMP_READS
  if (nread >= 0) {
    dump_hex(buf->base, nread);
  } else {
    ELOG("%s", uv_err_name(nread));
  }
#endif
  UvSocketHolder* socket = From(pipe);
  if (nread < 0 || nread == UV_EOF) {
    socket->OnEoF();
  } else {
    socket->OnFrame(buf->base, nread);
  }
  delete[] buf->base;
}

void UvSocketHolder::WriteCb(uv_write_t* req, int status) {
  WriteReqData* data = static_cast<WriteReqData*>(req->data);
  DLOG("%ld bytes written", data->_len);
  if (status != 0) {
    ELOG("session(%u) uv_write error: %s", SessionId(), uv_err_name(status));
  }
  delete data;
  delete req;
}

void UvSocketHolder::OnClosed(uv_handle_t* handle) {
  DLOG("socket closed");
  UvSocketHolder* holder = From(handle);
  holder->delegate()->OnClosed();
  delete holder;
}

/**
 * MARK: - Instance members
 */

bool UvSocketHolder::Ref() {
  uv_handle_t* handle = reinterpret_cast<uv_handle_t*>(&pipe_);
  uv_ref(handle);
  return true;
}

bool UvSocketHolder::Unref() {
  uv_handle_t* handle = reinterpret_cast<uv_handle_t*>(&pipe_);
  uv_unref(handle);
  return true;
}

bool UvSocketHolder::Write(char* data, size_t len) {
  DLOG("write data %zu", len);
  uv_write_t* req = new uv_write_t();
  req->data = new WriteReqData(this, data, len);
  uv_stream_t* pipe = reinterpret_cast<uv_stream_t*>(&pipe_);
  uv_buf_t buf = uv_buf_init(const_cast<char*>(data), len);
  const uv_buf_t bufs[] = {buf};
  if (uv_write(req, pipe, bufs, 1, WriteCb) != 0) {
    return false;
  }
  return true;
}

void UvSocketHolder::DisconnectAndDispose() {
  SocketHolder::DisconnectAndDispose();
  uv_handle_t* handle = reinterpret_cast<uv_handle_t*>(&pipe_);
  uv_close(handle, OnClosed);
}

}  // namespace ipc
}  // namespace aworker
