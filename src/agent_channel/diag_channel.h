#ifndef SRC_AGENT_CHANNEL_DIAG_CHANNEL_H_
#define SRC_AGENT_CHANNEL_DIAG_CHANNEL_H_
#include <memory>
#include <string>
#include <vector>
#include "uv.h"
#include "watchdog.h"

namespace aworker {

class AgentDiagChannel;

class AgentDiagChannelInspectorDelegate {
 public:
  virtual void AssignDiagChannel(AgentDiagChannel* channel) = 0;
  virtual void StartSession(int session_id, const std::string& target_id) = 0;
  virtual void EndSession(int session_id) = 0;
  virtual void MessageReceived(int session_id, const std::string& message) = 0;
  virtual std::vector<std::string> GetTargetIds() = 0;
  virtual std::string GetTargetTitle(const std::string& id) = 0;
  virtual std::string GetTargetUrl(const std::string& id) = 0;
  virtual ~AgentDiagChannelInspectorDelegate() = default;
};

class AgentDiagChannel : public WatchdogEntry {
 public:
  explicit AgentDiagChannel(Watchdog* watchdog, std::string credential);
  ~AgentDiagChannel() override = default;

  inline std::string& credential() { return credential_; }

  virtual void Start(uv_loop_t* loop) = 0;
  virtual void Stop() = 0;

  virtual void AssignInspectorDelegate(
      std::unique_ptr<AgentDiagChannelInspectorDelegate> delegate) = 0;
  virtual void TerminateConnections() = 0;
  virtual void StopAcceptingNewConnections() = 0;
  virtual void Send(int session_id, std::string message) = 0;

  void ThreadEntry(uv_loop_t* loop) override;
  void ThreadAtExit() override;

 private:
  std::string credential_;
};

}  // namespace aworker

#endif  // SRC_AGENT_CHANNEL_DIAG_CHANNEL_H_
