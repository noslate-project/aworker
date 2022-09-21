#include "loop_latency_watchdog.h"
#include "error_handling.h"

namespace aworker {

LoopLatencyWatchdog::LoopLatencyWatchdog(Immortal* immortal,
                                         uint64_t latency_limit_ms)
    : immortal_(immortal), latency_limit_ms_(latency_limit_ms) {
  immortal->watchdog()->RegisterEntry(this);
}

void LoopLatencyWatchdog::ThreadEntry(uv_loop_t* loop) {
  CHECK_EQ(uv_timer_init(loop, &timer_), 0);
  CHECK_EQ(uv_async_init(loop, &async_, AsyncCallback), 0);
}

void LoopLatencyWatchdog::ThreadAtExit() {
  uv_timer_stop(&timer_);
  uv_close(reinterpret_cast<uv_handle_t*>(&async_), nullptr);
  uv_close(reinterpret_cast<uv_handle_t*>(&timer_), nullptr);
}

void LoopLatencyWatchdog::OnPrepare() {
  UniqueLock scoped_lock(idle_mutex_);
  idle_ = true;
}

void LoopLatencyWatchdog::OnCheck() {
  UniqueLock scoped_lock(idle_mutex_);
  idle_ = false;
  uv_async_send(&async_);
}

// static
void LoopLatencyWatchdog::AsyncCallback(uv_async_t* handle) {
  LoopLatencyWatchdog* watchdog =
      ContainerOf(&LoopLatencyWatchdog::async_, handle);

  CHECK_EQ(uv_timer_start(&watchdog->timer_,
                          &LoopLatencyWatchdog::TimerCallback,
                          watchdog->latency_limit_ms_,
                          0),
           0);
}

// static
void LoopLatencyWatchdog::TimerCallback(uv_timer_t* handle) {
  LoopLatencyWatchdog* watchdog =
      ContainerOf(&LoopLatencyWatchdog::timer_, handle);
  UniqueLock scoped_lock(watchdog->idle_mutex_);

  if (watchdog->idle_) {
    return;
  }

  watchdog->immortal_->RequestInterrupt([]() {
    FatalError("LoopLatencyWatchdog", "Loop latency reached limit");
  });
}

}  // namespace aworker
