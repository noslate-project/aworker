#pragma once
#include "immortal.h"
#include "inspector/inspector_io.h"
#include "v8-inspector.h"
#include "v8.h"

#if !HAVE_INSPECTOR
#error("This header can only be used when inspector is enabled")
#endif

namespace aworker {
namespace inspector {
class AworkerInspectorClient;

struct ContextInfo {
  explicit ContextInfo(const std::string& name) : name(name) {}
  const std::string name;
  std::string origin;
  bool is_default;
};

class InspectorSession {
 public:
  virtual ~InspectorSession() = default;
  virtual void Dispatch(const v8_inspector::StringView& message) = 0;
};

class InspectorSessionDelegate {
 public:
  virtual ~InspectorSessionDelegate() = default;
  virtual void SendMessageToFrontend(
      const v8_inspector::StringView& message) = 0;
};

class InspectorAgent {
 public:
  explicit InspectorAgent(Immortal* immortal);
  ~InspectorAgent();

  // Create client_
  bool Start(const std::string& script_name);
  // Stop
  void Stop();

  // Returns true if the Aworker inspector is actually in use. It will be true
  // if the user explicitly opted into inspector (e.g. with the
  // --inspect-brk command line flag)
  bool IsActive();

  // Option is set to wait for session connection
  bool WillWaitForConnect();
  // Blocks till frontend connects and sends "runIfWaitingForDebugger"
  void WaitForConnect();
  // Blocks till all the sessions with "WaitForDisconnectOnShutdown" disconnect
  void WaitForDisconnect();
  bool HasConnectedSessions();
  void ReportUncaughtException(v8::Local<v8::Value> error,
                               v8::Local<v8::Message> message);

  // Called to create inspector sessions that can be used from the same thread.
  // The inspector responds by using the delegate to send messages back.
  std::unique_ptr<InspectorSession> Connect(
      std::unique_ptr<InspectorSessionDelegate> delegate,
      bool prevent_shutdown);

  void PauseOnNextJavascriptStatement(const std::string& reason);

  // Can only be called from the main thread.
  bool StartInspectorIo();
  void StopInspectorIo();

  void ContextCreated(v8::Local<v8::Context> context, const ContextInfo& info);

  Immortal* immortal() { return immortal_; }

 private:
  Immortal* immortal_;
  // Encapsulates majority of the Inspector functionality
  std::shared_ptr<AworkerInspectorClient> client_;
  // Interface for transports, e.g. Noslated server
  std::unique_ptr<InspectorIo> io_;
  std::string script_name_;
};

}  // namespace inspector
}  // namespace aworker
