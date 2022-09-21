#include <unistd.h>
#include <uv.h>

#include "aworker_logger.h"
#include "ipc/ipc_socket.h"
#include "ipc/ipc_socket_server.h"
#include "ipc/ipc_socket_uv.h"
#include "util.h"

namespace aworker {
namespace ipc {
namespace server {

using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::unique_ptr;

void AliceSocketServer::AliceSocketServerCloseCallback(uv_handle_t* handle) {
  uv_pipe_t* it = reinterpret_cast<uv_pipe_t*>(handle);
  AliceSocketServer* server = ContainerOf(&AliceSocketServer::server_pipe_, it);
  server->service_->Closed();
  delete server;
}

void AliceSocketServer::Initialize() {
#if defined(__POSIX__)
  // Disable SIGPIPE and handle them in place.
  {
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, nullptr);
  }
#endif
  unlink(server_pipe_name_.c_str());
  CHECK_EQ(0, uv_pipe_init(loop_.get(), &server_pipe_, 0));
  int ret = uv_pipe_bind(&server_pipe_, server_pipe_name_.c_str());
  if (ret != 0) {
    ELOG("uv_pipe_bind(%s) failed for error(%d, %s)",
         server_pipe_name_.c_str(),
         ret,
         uv_strerror(ret));
    ABORT();
  }
  // 511 is the value used by the 'net' module by default
  ret = uv_listen(
      reinterpret_cast<uv_stream_t*>(&server_pipe_), 511, ListenCallback);
  if (ret != 0) {
    ELOG("uv_listen failed for error(%d, %s)", ret, uv_strerror(ret));
    ABORT();
  }
  DLOG("alice server started on: %s", server_pipe_name_.c_str());
}

void AliceSocketServer::Close() {
  DLOG("closing alice server");
  connected_sessions_.erase(connected_sessions_.begin(),
                            connected_sessions_.end());
  uv_handle_t* handle = reinterpret_cast<uv_handle_t*>(&server_pipe_);
  uv_close(handle, AliceSocketServerCloseCallback);
}

int AliceSocketServer::Run() {
  bool more = false;
  do {
    uv_run(loop_.get(), UV_RUN_DEFAULT);

    // Emit `beforeExit` if the loop became alive either after emitting
    // event, or after running some callbacks.
    more = uv_loop_alive(loop_.get());
  } while (more == true);
  return 0;
}

void AliceSocketServer::Ref() {
  uv_ref(reinterpret_cast<uv_handle_t*>(&server_pipe_));
}

void AliceSocketServer::Unref() {
  uv_unref(reinterpret_cast<uv_handle_t*>(&server_pipe_));
}

void AliceSocketServer::Accept() {
  auto session = std::make_unique<SocketSession>(next_session_id_++);
  DLOG("new connection: id(%u)", session->id());

  auto delegate =
      std::make_shared<ServerDelegate>(this, service_, session->id());

  uv_stream_t* server_stream = reinterpret_cast<uv_stream_t*>(&server_pipe_);
  UvSocketHolder::Pointer socket =
      UvSocketHolder::Accept(server_stream, std::move(delegate));
  if (socket) {
    session->Own(std::move(socket));
    connected_sessions_[session->id()] = std::move(session);
  }
}

void AliceSocketServer::ListenCallback(uv_stream_t* server, int status) {
  AliceSocketServer* ami = ContainerOf(&AliceSocketServer::server_pipe_,
                                       reinterpret_cast<uv_pipe_t*>(server));
  CHECK_EQ(0, status);
  ami->Accept();
}

void ServerDelegate::OnError() {
  ELOG("connection on error: id(%u)", session_id());
}

void ServerDelegate::OnFinished() {
  DLOG("connection finished: id(%u)", session_id());
  server_->CloseSession(session_id());
  service()->Disconnected(session_id());
}

void ServerDelegate::OnClosed() {
  DLOG("socket closed: id(%u)", session_id());
  // Do nothing here. Server service doesn't care about closure of each session.
}

}  // namespace server
}  // namespace ipc
}  // namespace aworker
