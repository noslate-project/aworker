#ifndef SRC_IPC_IPC_SOCKET_SERVER_H_
#define SRC_IPC_IPC_SOCKET_SERVER_H_
#include <uv.h>
#include <map>
#include <string>
#include <vector>
#include "interface.h"
#include "ipc/ipc_delegate.h"
#include "ipc/ipc_delegate_impl.h"
#include "ipc/ipc_service.h"
#include "ipc/ipc_socket.h"
#include "ipc/ipc_socket_uv.h"
#include "ipc/uv_loop.h"

namespace aworker {
namespace ipc {
namespace server {

class SocketSession {
 public:
  explicit SocketSession(SessionId id) : id_(id) {}
  ~SocketSession() = default;
  void Close() { socket_.reset(); }
  void Own(UvSocketHolder::Pointer socket) { socket_ = std::move(socket); }
  SessionId id() const { return id_; }
  UvSocketHolder* socket() { return socket_.get(); }

 private:
  const SessionId id_;
  UvSocketHolder::Pointer socket_ = nullptr;
};

class NoslatedSocketServer {
 public:
  NoslatedSocketServer(std::shared_ptr<uv_loop_t> loop,
                    std::string server_pipe_name,
                    std::shared_ptr<NoslatedService> service)
      : service_(service),
        loop_(std::move(loop)),
        server_pipe_name_(server_pipe_name) {}
  void Initialize();
  void Close();
  int Run();
  void Accept();
  void Ref();
  void Unref();

  inline void CloseSession(SessionId session_id) {
    auto it = connected_sessions_.find(session_id);
    if (it != connected_sessions_.end()) {
      connected_sessions_.erase(it);
    }
  }

  inline std::weak_ptr<SocketSession> Session(SessionId session_id) {
    auto it = connected_sessions_.find(session_id);
    return it == connected_sessions_.end()
               ? std::weak_ptr<SocketSession>()
               : std::weak_ptr<SocketSession>(it->second);
  }

  inline uv_loop_t* loop() { return loop_.get(); }

 private:
  ~NoslatedSocketServer() = default;
  static void NoslatedSocketServerCloseCallback(uv_handle_t* handle);
  static void ListenCallback(uv_stream_t* server, int status);

  std::shared_ptr<NoslatedService> service_;
  std::shared_ptr<uv_loop_t> loop_;
  std::string server_pipe_name_;
  uv_pipe_t server_pipe_;
  std::map<SessionId, SessionId> foobar_;
  std::map<SessionId, std::shared_ptr<SocketSession>> connected_sessions_;
  SessionId next_session_id_ = 0;
};

class ServerDelegate : public DelegateImpl {
 public:
  ServerDelegate(NoslatedSocketServer* server,
                 std::shared_ptr<NoslatedService> service,
                 SessionId session_id)
      : DelegateImpl(service, std::make_shared<UvLoop>(server->loop())),
        session_id_(session_id),
        server_(server) {}
  ~ServerDelegate() = default;
  void OnError() override;
  void OnFinished() override;
  void OnClosed() override;

  std::shared_ptr<SocketHolder> socket() override {
    if (auto session = server_->Session(session_id()).lock()) {
      return unowned_ptr(session->socket());
    }
    return nullptr;
  }

  SessionId session_id() override { return session_id_; }

 private:
  std::weak_ptr<SocketSession> Session() {
    return server_->Session(session_id());
  }

  SessionId session_id_;
  NoslatedSocketServer* server_;
};

}  // namespace server
}  // namespace ipc
}  // namespace aworker

#endif  // SRC_IPC_IPC_SOCKET_SERVER_H_
