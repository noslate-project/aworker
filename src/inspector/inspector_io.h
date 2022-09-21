#pragma once

#if !HAVE_INSPECTOR
#error("This header can only be used when inspector is enabled")
#endif

#include <condition_variable>  // NOLINT(build/c++11)
#include <cstddef>
#include <memory>
#include <mutex>   // NOLINT(build/c++11)
#include <thread>  // NOLINT(build/c++11)

#include "agent_channel/diag_channel.h"
#include "watchdog.h"

namespace aworker {
namespace inspector {

class MainThreadHandle;
class RequestQueueHandle;

class InspectorIo {
  class InspectorIoWatchdog;

 public:
  // Start the inspector agent thread, waiting for it to initialize
  // bool Start();
  // Returns empty pointer if thread was not started
  static std::unique_ptr<InspectorIo> Start(
      Watchdog* watchdog,
      AgentDiagChannel* diag_channel,
      std::shared_ptr<MainThreadHandle> main_thread,
      const std::string& script_name);

  // Will block till the transport thread shuts down
  ~InspectorIo();

  void StopAcceptingNewConnections();

 private:
  InspectorIo(Watchdog* watchdog,
              AgentDiagChannel* diag_channel,
              std::shared_ptr<MainThreadHandle> handle,
              const std::string& script_name);

  void ThreadEntry(uv_loop_t* loop);

  AgentDiagChannel* diag_channel_;
  // This is a thread-safe object that will post async tasks. It lives as long
  // as an Inspector object lives (almost as long as an Isolate).
  std::shared_ptr<MainThreadHandle> main_thread_;
  // Used to post on a frontend interface thread, lives while the server is
  // running
  std::shared_ptr<RequestQueueHandle> request_queue_;

  std::string script_name_;
  // May be accessed from any thread
  const std::string id_;
};

}  // namespace inspector
}  // namespace aworker
