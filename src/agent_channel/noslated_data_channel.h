#ifndef SRC_AGENT_CHANNEL_NOSLATED_DATA_CHANNEL_H_
#define SRC_AGENT_CHANNEL_NOSLATED_DATA_CHANNEL_H_
#include "agent_channel/data_channel.h"
#include "aworker_binding.h"
#include "ipc/interface.h"
#include "ipc/ipc_delegate_impl.h"
#include "ipc/ipc_service.h"
#include "ipc/ipc_socket_uv.h"
#include "ipc/uv_loop.h"

namespace aworker {
namespace agent {

using namespace ::aworker::ipc;  // NOLINT(build/namespaces)
using std::unique_ptr;

class NoslatedDataChannel : public AgentDataChannel, public NoslatedService {
 public:
  NoslatedDataChannel(Immortal* immortal,
                      std::string server_path,
                      std::string credential,
                      bool refed);
  virtual ~NoslatedDataChannel();

  template <void (NoslatedDataChannel::*func)(
      std::unique_ptr<RpcController> controller,
      const v8::Local<v8::Object> params)>
  static AWORKER_METHOD(JsCall);
  void Callback(const uint32_t id,
                const v8::Local<v8::Value> exception,
                const v8::Local<v8::Value> params);

  void Emit(const uint32_t id,
            const std::string& event,
            const v8::Local<v8::Value> params);
  bool Feedback(const uint32_t id,
                const int32_t code,
                const v8::Local<v8::Object> params);

  void Ref() override;
  void Unref() override;

  void Trigger(unique_ptr<RpcController> controller,
               unique_ptr<TriggerRequestMessage> req,
               unique_ptr<TriggerResponseMessage> response,
               Closure<TriggerResponseMessage> closure) override;
  void StreamPush(unique_ptr<RpcController> controller,
                  unique_ptr<StreamPushRequestMessage> req,
                  unique_ptr<StreamPushResponseMessage> response,
                  Closure<StreamPushResponseMessage> closure) override;
  void CollectMetrics(unique_ptr<RpcController> controller,
                      unique_ptr<CollectMetricsRequestMessage> req,
                      unique_ptr<CollectMetricsResponseMessage> response,
                      Closure<CollectMetricsResponseMessage> closure) override;
  void ResourceNotification(
      unique_ptr<RpcController> controller,
      unique_ptr<ResourceNotificationRequestMessage> req,
      unique_ptr<ResourceNotificationResponseMessage> response,
      Closure<ResourceNotificationResponseMessage> closure) override;

  void OnError();
  void OnConnect(UvSocketHolder::Pointer socket);
  void Disconnected(SessionId session_id) override;
  void Closed() override;

  void CallFetch(unique_ptr<RpcController> controller,
                 const v8::Local<v8::Object> params);
  void CallFetchAbort(unique_ptr<RpcController> controller,
                      const v8::Local<v8::Object> params);
  void CallStreamOpen(unique_ptr<RpcController> controller,
                      const v8::Local<v8::Object> params);
  void CallStreamPush(unique_ptr<RpcController> controller,
                      const v8::Local<v8::Object> params);
  void CallDaprInvoke(unique_ptr<RpcController> controller,
                      const v8::Local<v8::Object> params);
  void CallDaprBinding(unique_ptr<RpcController> controller,
                       const v8::Local<v8::Object> params);
  void CallExtensionBinding(unique_ptr<RpcController> controller,
                            const v8::Local<v8::Object> params);
  void CallResourcePut(unique_ptr<RpcController> controller,
                       const v8::Local<v8::Object> params);

 protected:
  std::weak_ptr<SocketDelegate> socket_delegate(SessionId session_id) override {
    return delegate_;
  };

 private:
  class ClientDelegate : public DelegateImpl {
   public:
    ClientDelegate(std::shared_ptr<NoslatedDataChannel> channel,
                   std::shared_ptr<UvLoop> loop)
        : DelegateImpl(channel, loop), channel_(channel) {}
    std::shared_ptr<SocketHolder> socket() override {
      if (channel_->socket_ == nullptr) {
        return nullptr;
      }
      return unowned_ptr(channel_->socket_.get());
    }

    void OnError() override { channel_->OnError(); };

   private:
    std::shared_ptr<NoslatedDataChannel> channel_;
  };

  v8::Local<v8::Value> ErrorMessageToJsError(
      CanonicalCode code, unique_ptr<ErrorResponseMessage> error);

  uv_loop_t* loop_;
  UvSocketHolder::Pointer socket_ = nullptr;
  std::shared_ptr<ClientDelegate> delegate_;
  std::map<RequestId, std::function<void(int32_t, const v8::Local<v8::Object>)>>
      callbacks_;
  uint32_t ref_count_ = 0;
  bool closed_ = false;
};

}  // namespace agent
}  // namespace aworker

#endif  // SRC_AGENT_CHANNEL_NOSLATED_DATA_CHANNEL_H_
