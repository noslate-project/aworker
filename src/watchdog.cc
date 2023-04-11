#include "watchdog.h"
#include "debug_utils.h"
#include "utils/async_primitives.h"

namespace aworker {

Watchdog::Watchdog() : thread_(), started_(false) {}

Watchdog::~Watchdog() {
  if (started_) {
    stop_async_->Send();
    thread_.join();
    started_ = false;
  }
}

void Watchdog::RegisterEntry(WatchdogEntry* entry) {
  UniqueLock scoped_lock(thread_start_lock_);
  pending_entries_.push_back(entry);
  if (started_ && start_async_) {
    start_async_->Send();
  }
}

void Watchdog::StartIfNeeded() {
  if (started_) {
    return;
  }
  if (pending_entries_.size() == 0) {
    return;
  }
  UniqueLock scoped_lock(thread_start_lock_);
  thread_ = std::thread([this]() { ThreadMain(); });
  thread_start_condition_.wait(scoped_lock);
  started_ = true;
}

void Watchdog::ThreadMain() {
  uv_loop_init(&loop_);

  {
    ScopedLock scoped_lock(thread_start_lock_);

    start_async_ = UvAsync<std::function<void()>>::Create(
        &loop_, std::bind(&Watchdog::StartRequest, this));
    stop_async_ = UvAsync<std::function<void()>>::Create(
        &loop_, std::bind(&Watchdog::StopRequest, this));

    for (auto it : pending_entries_) {
      it->ThreadEntry(&loop_);
      started_entries_.push_back(it);
    }
    pending_entries_ = {};

    thread_start_condition_.notify_all();
  }
  uv_run(&loop_, UV_RUN_DEFAULT);

  CheckedUvLoopClose(&loop_);
}

void Watchdog::StartRequest() {
  ScopedLock scoped_lock(thread_start_lock_);
  for (auto it : pending_entries_) {
    it->ThreadEntry(&loop_);
    started_entries_.push_back(it);
  }
  pending_entries_ = {};
}

void Watchdog::StopRequest() {
  for (auto it : started_entries_) {
    it->ThreadAtExit();
  }
  start_async_.reset();
  stop_async_.reset();
}

SignalWatchdog::SignalWatchdog(Watchdog* watchdog, int signum)
    : signum_(signum) {
  watchdog->RegisterEntry(this);
}

void SignalWatchdog::ThreadEntry(uv_loop_t* loop) {
  CHECK_EQ(uv_signal_init(loop, &signal_), 0);
  CHECK_EQ(uv_signal_start(&signal_, &SignalWatchdog::SignalCallback, signum_),
           0);
}

void SignalWatchdog::ThreadAtExit() {
  uv_signal_stop(&signal_);
  uv_close(reinterpret_cast<uv_handle_t*>(&signal_), nullptr);
}

void SignalWatchdog::SignalCallback(uv_signal_t* handle, int signum) {
  SignalWatchdog* wrap = ContainerOf(&SignalWatchdog::signal_, handle);
  wrap->OnSignal();
}

}  // namespace aworker
