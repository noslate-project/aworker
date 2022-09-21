#ifndef SRC_WATCHDOG_H_
#define SRC_WATCHDOG_H_

#include <condition_variable>  // NOLINT(build/c++11)
#include <functional>
#include <list>
#include <mutex>   // NOLINT(build/c++11)
#include <thread>  // NOLINT(build/c++11)

#include "util.h"
#include "utils/async_primitives.h"
#include "uv.h"

namespace aworker {

class WatchdogEntry {
 public:
  virtual void ThreadEntry(uv_loop_t* loop) = 0;
  virtual void ThreadAtExit() {}

  virtual ~WatchdogEntry() = default;
};

class Watchdog {
 public:
  Watchdog();
  ~Watchdog();

  AWORKER_DISALLOW_ASSIGN_COPY(Watchdog)

  void RegisterEntry(WatchdogEntry* entry);

  void StartIfNeeded();

  inline bool started() { return started_; }

 private:
  void ThreadMain();
  void StartRequest();
  void StopRequest();

  // The IO thread runs its own event loop to implement the server off
  // the main thread.
  std::thread thread_;
  bool started_;

  // For setting up interthread communications
  std::mutex thread_start_lock_;
  std::condition_variable thread_start_condition_;

  std::list<WatchdogEntry*> started_entries_;
  std::list<WatchdogEntry*> pending_entries_;
  uv_loop_t loop_;

  UvAsync<std::function<void()>>::Ptr start_async_;
  UvAsync<std::function<void()>>::Ptr stop_async_;
};

class SignalWatchdog : public WatchdogEntry {
 public:
  SignalWatchdog(Watchdog* watchdog, int signum);

  AWORKER_DISALLOW_ASSIGN_COPY(SignalWatchdog)

  void ThreadEntry(uv_loop_t* loop) override;
  void ThreadAtExit() override;

 protected:
  virtual void OnSignal() = 0;

 private:
  static void SignalCallback(uv_signal_t* handle, int signum);

  uv_signal_t signal_;
  int signum_;
};

}  // namespace aworker

#endif  // SRC_WATCHDOG_H_
