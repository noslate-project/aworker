#include "ipc/ipc_service.h"

namespace aworker {
namespace ipc {
using std::make_unique;
using std::move;
using std::unique_ptr;

#define V(TYPE)                                                                \
  void AliceService::Request(unique_ptr<RpcController> controller,             \
                             unique_ptr<TYPE##RequestMessage> req,             \
                             Closure<TYPE##ResponseMessage> closure) {         \
    Request_(move(controller),                                                 \
             RequestKind::TYPE,                                                \
             move(req),                                                        \
             [closure = move(closure)](CanonicalCode code,                     \
                                       unique_ptr<ErrorResponseMessage> error, \
                                       unique_ptr<Message> body) {             \
               auto it = unique_ptr<TYPE##ResponseMessage>(                    \
                   static_cast<TYPE##ResponseMessage*>(body.release()));       \
               closure(code, move(error), move(it));                           \
             });                                                               \
  }
ALICE_REQUEST_TYPES(V)
#undef V

void AliceService::Request_(unique_ptr<RpcController> controller,
                            RequestKind kind,
                            unique_ptr<Message> req,
                            Closure<Message> closure) {
  if (auto it = socket_delegate(controller->session_id()).lock()) {
    it->Request(controller->request_id(),
                kind,
                move(req),
                move(closure),
                controller->timeout());
  } else {
    auto error = make_unique<ErrorResponseMessage>();
    error->set_message("Connection has been reset");
    closure(CanonicalCode::CONNECTION_RESET, move(error), nullptr);
  }
}

}  // namespace ipc
}  // namespace aworker
