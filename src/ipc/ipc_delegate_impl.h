#ifndef SRC_IPC_IPC_DELEGATE_IMPL_H_
#define SRC_IPC_IPC_DELEGATE_IMPL_H_
#include "ipc/ipc_delegate.h"
#include "ipc/ipc_service.h"
#include "ipc/ipc_socket.h"
#include "ipc/uv_loop.h"

namespace aworker {
namespace ipc {
class DelegateImpl : public SocketDelegate,
                     public std::enable_shared_from_this<DelegateImpl> {
 public:
  DelegateImpl(std::shared_ptr<AliceService> service,
               std::shared_ptr<EventLoop> loop)
      : service_(service), loop_(loop) {}
  ~DelegateImpl();

  void OnRequest(RequestId id,
                 RequestKind kind,
                 std::unique_ptr<Message> body) override;
  void OnResponse(RequestId id,
                  RequestKind kind,
                  CanonicalCode code,
                  std::unique_ptr<Message> body) override;
  void OnError() override;
  void OnFinished() override;
  void OnClosed() override;

  void Request(RequestId id,
               RequestKind kind,
               std::unique_ptr<Message> body,
               Closure<Message> callback,
               uint64_t timeout) override;

  virtual std::shared_ptr<SocketHolder> socket() { return nullptr; }
  virtual SessionId session_id() { return 0; }

  std::shared_ptr<AliceService> service() { return service_; }

 private:
  std::weak_ptr<DelegateImpl> weak_from_this() { return shared_from_this(); }
  void SetCallback(RequestId id, Closure<Message> callback, Timer* timer);
  std::shared_ptr<AliceService> service_;
  std::shared_ptr<EventLoop> loop_;
  std::map<RequestId, Closure<Message>> callbacks_;
  std::map<RequestId, Timer*> timers_;
};
}  // namespace ipc
}  // namespace aworker

#endif  // SRC_IPC_IPC_DELEGATE_IMPL_H_
