#include "agent_channel/diag_channel.h"

namespace aworker {

AgentDiagChannel::AgentDiagChannel(Watchdog* watchdog, std::string credential)
    : credential_(credential) {
  watchdog->RegisterEntry(this);
}

void AgentDiagChannel::ThreadEntry(uv_loop_t* loop) {
  Start(loop);
}

void AgentDiagChannel::ThreadAtExit() {
  Stop();
}

}  // namespace aworker
