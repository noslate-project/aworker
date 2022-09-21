#ifndef SRC_AGENT_CHANNEL_ALICE_DIAG_CHANNEL_H_
#define SRC_AGENT_CHANNEL_ALICE_DIAG_CHANNEL_H_
#include "diag_channel.h"
#include "immortal.h"
#include "ipc/interface.h"
#include "ipc/ipc_delegate_impl.h"
#include "ipc/ipc_service.h"
#include "ipc/ipc_socket_uv.h"
#include "ipc/uv_loop.h"

namespace aworker {
namespace agent {

using namespace ::aworker::ipc;  // NOLINT(build/namespaces)

class AliceDiagChannel : public AgentDiagChannel, public AliceService {
 public:
  explicit AliceDiagChannel(Immortal* immortal,
                            std::string server_path,
                            std::string credential);
  ~AliceDiagChannel();

  void Start(uv_loop_t* loop) override;
  void Stop() override;

  void AssignInspectorDelegate(
      std::unique_ptr<AgentDiagChannelInspectorDelegate> delegate) override;
  void TerminateConnections() override;
  void StopAcceptingNewConnections() override;
  void Send(int session_id, std::string message) override;

  void InspectorStart(std::unique_ptr<RpcController> controller,
                      const std::unique_ptr<InspectorStartRequestMessage> req,
                      std::unique_ptr<InspectorStartResponseMessage> response,
                      Closure<InspectorStartResponseMessage> closure) override;
  void InspectorStartSession(
      std::unique_ptr<RpcController> controller,
      const std::unique_ptr<InspectorStartSessionRequestMessage> req,
      std::unique_ptr<InspectorStartSessionResponseMessage> response,
      Closure<InspectorStartSessionResponseMessage> closure) override;
  void InspectorEndSession(
      std::unique_ptr<RpcController> controller,
      const std::unique_ptr<InspectorEndSessionRequestMessage> req,
      std::unique_ptr<InspectorEndSessionResponseMessage> response,
      Closure<InspectorEndSessionResponseMessage> closure) override;
  void InspectorGetTargets(
      std::unique_ptr<RpcController> controller,
      const std::unique_ptr<InspectorGetTargetsRequestMessage> req,
      std::unique_ptr<InspectorGetTargetsResponseMessage> response,
      Closure<InspectorGetTargetsResponseMessage> closure) override;
  void InspectorCommand(
      std::unique_ptr<RpcController> controller,
      const std::unique_ptr<InspectorCommandRequestMessage> req,
      std::unique_ptr<InspectorCommandResponseMessage> response,
      Closure<InspectorCommandResponseMessage> closure) override;

  void TracingStart(std::unique_ptr<RpcController> controller,
                    std::unique_ptr<TracingStartRequestMessage> req,
                    std::unique_ptr<TracingStartResponseMessage> response,
                    Closure<TracingStartResponseMessage> closure) override;
  void TracingStop(std::unique_ptr<RpcController> controller,
                   std::unique_ptr<TracingStopRequestMessage> req,
                   std::unique_ptr<TracingStopResponseMessage> response,
                   Closure<TracingStopResponseMessage> closure) override;

  void Disconnected(SessionId session_id) override;
  void OnError();

 protected:
  std::weak_ptr<SocketDelegate> socket_delegate(SessionId session_id) override {
    return socket_delegate_;
  };

 private:
  class ClientDelegate : public DelegateImpl {
   public:
    ClientDelegate(std::shared_ptr<AliceDiagChannel> channel,
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
    std::shared_ptr<AliceDiagChannel> channel_;
  };

  void OnConnect(UvSocketHolder::Pointer socket);
  void SendInspectorStarted();

  Immortal* immortal_;
  bool connected_ = false;
  bool stopping_ = false;
  uint32_t opened_session_count_ = 0;
  std::string server_path_;
  uv_loop_t* loop_;
  std::unique_ptr<AgentDiagChannelInspectorDelegate> delegate_;
  UvSocketHolder::Pointer socket_ = nullptr;
  std::shared_ptr<ClientDelegate> socket_delegate_;
};

}  // namespace agent
}  // namespace aworker

#endif  // SRC_AGENT_CHANNEL_ALICE_DIAG_CHANNEL_H_
