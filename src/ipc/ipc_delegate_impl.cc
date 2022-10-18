#include "ipc/ipc_delegate_impl.h"
#include "aworker_logger.h"
#include "util.h"

namespace aworker {
namespace ipc {
using std::make_unique;
using std::move;
using std::unique_ptr;

DelegateImpl::~DelegateImpl() {
  for (auto it = timers_.begin(); it != timers_.end(); it++) {
    loop_->ClearTimeout(it->second);
  }
  timers_.erase(timers_.begin(), timers_.end());

  for (auto it = callbacks_.begin(); it != callbacks_.end(); it++) {
    it->second(CanonicalCode::CONNECTION_RESET, nullptr, nullptr);
  }
  callbacks_.erase(callbacks_.begin(), callbacks_.end());
}

void DelegateImpl::OnRequest(RequestId id,
                             RequestKind kind,
                             unique_ptr<Message> body) {
  unique_ptr<RpcController> rpc_controller =
      RpcController::NewWithRequestId(session_id(), id);
  DLOG("dispatching request(%u) with kind: %d", id, kind);

#define V(TYPE)                                                                \
  case RequestKind::TYPE: {                                                    \
    service()->TYPE(move(rpc_controller),                                      \
                    std::unique_ptr<TYPE##RequestMessage>(                     \
                        static_cast<TYPE##RequestMessage*>(body.release())),   \
                    make_unique<TYPE##ResponseMessage>(),                      \
                    [weak_self = weak_from_this(), id, kind](                  \
                        CanonicalCode code,                                    \
                        unique_ptr<ErrorResponseMessage> error,                \
                        unique_ptr<TYPE##ResponseMessage> resp) {              \
                      auto self = weak_self.lock();                            \
                      if (self == nullptr) {                                   \
                        return;                                                \
                      }                                                        \
                      if (auto it = self->socket()) {                          \
                        unique_ptr<Message> msg;                               \
                        if (code != CanonicalCode::OK) {                       \
                          msg = error != nullptr                               \
                                    ? move(error)                              \
                                    : make_unique<ErrorResponseMessage>();     \
                        } else {                                               \
                          msg = resp != nullptr                                \
                                    ? move(resp)                               \
                                    : make_unique<TYPE##ResponseMessage>();    \
                        }                                                      \
                        it->SocketHolder::Write(                               \
                            MessageKind::Response, id, kind, move(msg), code); \
                      } else {                                                 \
                        ELOG("Unable to fetch a socket reference");            \
                      }                                                        \
                    });                                                        \
    break;                                                                     \
  }

  switch (kind) {
    NOSLATED_REQUEST_TYPES(V)
    default: {
      ELOG("unreconigzable request kind: %d", kind);
      DCHECK(false);
    }
  }

#undef V
}

void DelegateImpl::OnResponse(RequestId id,
                              RequestKind kind,
                              CanonicalCode code,
                              unique_ptr<Message> body) {
  DLOG("on response: id(%u)", id);
  auto it = callbacks_.find(id);
  if (it == callbacks_.end()) {
    DLOG("request(id: %u) callbacks not found, may be timed out", id);
    return;
  }

  auto timer_it = timers_.find(id);
  if (timer_it != timers_.end()) {
    loop_->ClearTimeout(timer_it->second);
    timers_.erase(timer_it);
  }

  if (it->second) {
    it->second(code,
               code != CanonicalCode::OK
                   ? unique_ptr<ErrorResponseMessage>(
                         static_cast<ErrorResponseMessage*>(body.release()))
                   : nullptr,
               code == CanonicalCode::OK ? move(body) : nullptr);
  }
  callbacks_.erase(it);
}

void DelegateImpl::OnError() {}

void DelegateImpl::OnFinished() {
  service_->Disconnected(0);
}

void DelegateImpl::OnClosed() {
  service_->Closed();
}

void DelegateImpl::Request(RequestId id,
                           RequestKind kind,
                           unique_ptr<Message> body,
                           Closure<Message> callback,
                           uint64_t timeout) {
  DLOG("got socket %p", socket().get());
  if (auto it = socket()) {
    auto timer = loop_->SetTimeout(
        [weak_self = weak_from_this(), id, kind, timeout](Timer*) {
          auto self = weak_self.lock();
          if (self == nullptr) {
            return;
          }
          ELOG("request(%d) timed out for %llu ms", id, timeout);
          self->OnResponse(id, kind, CanonicalCode::TIMEOUT, nullptr);
        },
        timeout);
    DLOG("set timeout %llu ms, %p", timeout, timer);
    SetCallback(id, move(callback), timer);

    it->SocketHolder::Write(
        MessageKind::Request, id, kind, move(body), CanonicalCode::OK);
  } else {
    ELOG("unable to fetch socket reference");
  }
}

void DelegateImpl::SetCallback(RequestId id,
                               Closure<Message> callback,
                               Timer* timer) {
  DLOG("set response callback id(%u)", id);
  CHECK(callbacks_.find(id) == callbacks_.end());
  CHECK(timers_.find(id) == timers_.end());
  callbacks_[id] = move(callback);
  timers_[id] = timer;
}

}  // namespace ipc
}  // namespace aworker
