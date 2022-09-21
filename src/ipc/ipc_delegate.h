#ifndef SRC_IPC_IPC_DELEGATE_H_
#define SRC_IPC_IPC_DELEGATE_H_
#include <unistd.h>
#include <functional>
#include <memory>
#include <vector>
#include "ipc/ipc_pb.h"

namespace aworker {
namespace ipc {
using SessionId = uint32_t;
using RequestId = uint32_t;
using StreamId = uint32_t;
using Message = google::protobuf::MessageLite;

template <typename T>
using Closure = std::function<void(
    CanonicalCode, std::unique_ptr<ErrorResponseMessage>, std::unique_ptr<T>)>;

class SocketDelegate {
 public:
  SocketDelegate() {}
  virtual ~SocketDelegate() = default;

  virtual void OnRequest(RequestId id,
                         RequestKind kind,
                         std::unique_ptr<Message> body) = 0;
  virtual void OnResponse(RequestId id,
                          RequestKind kind,
                          CanonicalCode code,
                          std::unique_ptr<Message> body) = 0;

  virtual void OnError() = 0;
  /**
   * When the socket reads an EOF, peer closed the socket.
   */
  virtual void OnFinished() = 0;
  /**
   * When the socket is closed, after which the socket is released.
   */
  virtual void OnClosed() = 0;

  virtual void Request(RequestId id,
                       RequestKind kind,
                       std::unique_ptr<Message> body,
                       Closure<Message> callback,
                       uint64_t timeout) = 0;
};

}  // namespace ipc

}  // namespace aworker

#endif  // SRC_IPC_IPC_DELEGATE_H_
