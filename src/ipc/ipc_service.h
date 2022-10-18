#ifndef SRC_IPC_IPC_SERVICE_H_
#define SRC_IPC_IPC_SERVICE_H_
#include <memory>
#include "ipc/ipc_delegate.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_pb.h"
#include "ipc/ipc_socket.h"

namespace aworker {
namespace ipc {

class NoslatedService;

class RpcController {
 public:
  static std::unique_ptr<RpcController> NewWithRequestId(RequestId request_id) {
    return std::make_unique<RpcController>(0, request_id, 0);
  }
  static std::unique_ptr<RpcController> NewWithRequestId(SessionId session_id,
                                                         RequestId request_id) {
    return std::make_unique<RpcController>(session_id, request_id, 0);
  }
  RpcController(SessionId session_id, RequestId request_id, uint64_t timeout)
      : session_id_(session_id), request_id_(request_id), timeout_(timeout) {}

  inline SessionId session_id() { return session_id_; }
  inline RequestId request_id() { return request_id_; }
  inline uint64_t timeout() { return timeout_; }

 private:
  SessionId session_id_;
  RequestId request_id_;
  uint64_t timeout_;

  friend NoslatedService;
};

class NoslatedService {
 public:
  NoslatedService() {}
  virtual ~NoslatedService() = default;

  std::unique_ptr<RpcController> NewControllerWithTimeout(uint64_t ms) {
    return std::make_unique<RpcController>(0, next_seq(), ms);
  }
  std::unique_ptr<RpcController> NewControllerWithTimeout(SessionId sessionId,
                                                          uint64_t ms) {
    return std::make_unique<RpcController>(sessionId, next_seq(), ms);
  }

  // As Client
#define V(TYPE)                                                                \
  void Request(std::unique_ptr<RpcController> controller,                      \
               std::unique_ptr<TYPE##RequestMessage> req,                      \
               Closure<TYPE##ResponseMessage> closure);
  NOSLATED_REQUEST_TYPES(V)
#undef V

  // As Server
#define V(TYPE)                                                                \
  virtual void TYPE(std::unique_ptr<RpcController> controller,                 \
                    std::unique_ptr<TYPE##RequestMessage> req,                 \
                    std::unique_ptr<TYPE##ResponseMessage> res,                \
                    Closure<TYPE##ResponseMessage> closure) {                  \
    auto error = std::make_unique<ErrorResponseMessage>();                     \
    error->set_message("Not Implemented");                                     \
    closure(CanonicalCode::NOT_IMPLEMENTED, move(error), nullptr);             \
  };
  NOSLATED_REQUEST_TYPES(V)
#undef V

  virtual void Disconnected(SessionId session_id) = 0;
  virtual void Closed() {}

 protected:
  virtual std::weak_ptr<SocketDelegate> socket_delegate(
      SessionId session_id) = 0;

 private:
  void Request_(std::unique_ptr<RpcController> controller,
                RequestKind kind,
                std::unique_ptr<Message> req,
                Closure<Message> closure);
  RequestId next_seq() {
    return seq_++;
  }
  RequestId seq_ = 0;
};

}  // namespace ipc
}  // namespace aworker

#endif  // SRC_IPC_IPC_SERVICE_H_
