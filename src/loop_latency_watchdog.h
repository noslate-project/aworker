#ifndef SRC_LOOP_LATENCY_WATCHDOG_H_
#define SRC_LOOP_LATENCY_WATCHDOG_H_

#include "immortal.h"
#include "watchdog.h"

namespace aworker {

class LoopLatencyWatchdog : public WatchdogEntry {
 public:
  LoopLatencyWatchdog(Immortal* immortal, uint64_t latency_limit_ms);

  AWORKER_DISALLOW_ASSIGN_COPY(LoopLatencyWatchdog)

  void ThreadEntry(uv_loop_t* loop) override;
  void ThreadAtExit() override;

  void OnPrepare();
  void OnCheck();

 private:
  static void AsyncCallback(uv_async_t* handle);
  static void TimerCallback(uv_timer_t* handle);

  Immortal* immortal_;

  uv_async_t async_;
  uv_timer_t timer_;
  uint64_t latency_limit_ms_;

  std::mutex idle_mutex_;
  bool idle_;
};

}  // namespace aworker

#endif  // SRC_LOOP_LATENCY_WATCHDOG_H_
