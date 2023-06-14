#ifndef SRC_LOOP_LATENCY_WATCHDOG_H_
#define SRC_LOOP_LATENCY_WATCHDOG_H_

#include "immortal.h"
#include "watchdog.h"

namespace aworker {

class LoopLatencyWatchdog : public WatchdogEntry {
 public:
  LoopLatencyWatchdog(Immortal* immortal,
                      uint64_t long_task_threshold_ms,
                      uint64_t fatal_latency_limit_ms);

  AWORKER_DISALLOW_ASSIGN_COPY(LoopLatencyWatchdog)

  void ThreadEntry(uv_loop_t* loop) override;
  void ThreadAtExit() override;

  void CallbackPrologue();
  void CallbackEpilogue();

 private:
  static void AsyncCallback(uv_async_t* handle);
  static void LongTaskTimerCallback(uv_timer_t* handle);
  static void FatalTimerCallback(uv_timer_t* handle);

  Immortal* immortal_;

  uv_async_t async_;
  uv_timer_t long_task_timer_;
  uv_timer_t fatal_timer_;
  uint64_t long_task_threshold_ms_;
  uint64_t fatal_latency_limit_ms_;

  std::mutex idle_mutex_;
  bool idle_;
};

}  // namespace aworker

#endif  // SRC_LOOP_LATENCY_WATCHDOG_H_
