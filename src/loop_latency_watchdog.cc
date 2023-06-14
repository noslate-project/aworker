#include "loop_latency_watchdog.h"
#include "debug_utils.h"
#include "error_handling.h"

namespace aworker {

LoopLatencyWatchdog::LoopLatencyWatchdog(Immortal* immortal,
                                         uint64_t long_task_threshold_ms,
                                         uint64_t fatal_latency_limit_ms)
    : immortal_(immortal),
      long_task_threshold_ms_(long_task_threshold_ms),
      fatal_latency_limit_ms_(fatal_latency_limit_ms) {
  immortal->watchdog()->RegisterEntry(this);
}

void LoopLatencyWatchdog::ThreadEntry(uv_loop_t* loop) {
  CHECK_EQ(uv_timer_init(loop, &long_task_timer_), 0);
  CHECK_EQ(uv_timer_init(loop, &fatal_timer_), 0);
  CHECK_EQ(uv_async_init(loop, &async_, AsyncCallback), 0);
}

void LoopLatencyWatchdog::ThreadAtExit() {
  uv_timer_stop(&long_task_timer_);
  uv_timer_stop(&fatal_timer_);
  uv_close(reinterpret_cast<uv_handle_t*>(&async_), nullptr);
  uv_close(reinterpret_cast<uv_handle_t*>(&long_task_timer_), nullptr);
  uv_close(reinterpret_cast<uv_handle_t*>(&fatal_timer_), nullptr);
}

void LoopLatencyWatchdog::CallbackPrologue() {
  UniqueLock scoped_lock(idle_mutex_);
  idle_ = false;
  uv_async_send(&async_);
}

void LoopLatencyWatchdog::CallbackEpilogue() {
  UniqueLock scoped_lock(idle_mutex_);
  idle_ = true;
}

// static
void LoopLatencyWatchdog::AsyncCallback(uv_async_t* handle) {
  LoopLatencyWatchdog* watchdog =
      ContainerOf(&LoopLatencyWatchdog::async_, handle);

  if (watchdog->long_task_threshold_ms_) {
    CHECK_EQ(uv_timer_start(&watchdog->long_task_timer_,
                            &LoopLatencyWatchdog::LongTaskTimerCallback,
                            watchdog->long_task_threshold_ms_,
                            0),
             0);
  }
  if (watchdog->fatal_latency_limit_ms_) {
    CHECK_EQ(uv_timer_start(&watchdog->fatal_timer_,
                            &LoopLatencyWatchdog::FatalTimerCallback,
                            watchdog->fatal_latency_limit_ms_,
                            0),
             0);
  }
}

// static
void LoopLatencyWatchdog::LongTaskTimerCallback(uv_timer_t* handle) {
  LoopLatencyWatchdog* watchdog =
      ContainerOf(&LoopLatencyWatchdog::long_task_timer_, handle);
  UniqueLock scoped_lock(watchdog->idle_mutex_);

  if (watchdog->idle_) {
    return;
  }

  watchdog->immortal_->RequestInterrupt([](Immortal* immortal,
                                           InterruptKind kind) {
    if (kind == InterruptKind::kJavaScript) {
      PrintJavaScriptStack(immortal->isolate(), "Loop latency reached limit");
    }
  });
}

// static
void LoopLatencyWatchdog::FatalTimerCallback(uv_timer_t* handle) {
  LoopLatencyWatchdog* watchdog =
      ContainerOf(&LoopLatencyWatchdog::fatal_timer_, handle);
  UniqueLock scoped_lock(watchdog->idle_mutex_);

  if (watchdog->idle_) {
    return;
  }

  watchdog->immortal_->RequestInterrupt(
      [](Immortal* immortal, InterruptKind kind) {
        FatalError("LoopLatencyWatchdog", "Loop latency reached limit");
      });
}

}  // namespace aworker
