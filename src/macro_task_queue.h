#pragma once

#include <memory>
#include <queue>
#include <string>

#include "util.h"
#include "uv.h"

namespace aworker {

class MacroTask {
 public:
  inline MacroTask() : _is_done(false), _is_error(false) {}
  virtual inline ~MacroTask() {}

  virtual void OnWorkTick() = 0;
  virtual void OnDone() {}

  inline bool is_done() { return _is_done; }
  inline bool has_error() { return _is_error; }
  inline std::string last_error() { return _error; }

 protected:
  virtual void Done() { _is_done = true; }
  virtual void Fail(const char* error) {
    _is_error = true;
    _error = std::string(error);
  }

  bool _is_done;
  bool _is_error;
  std::string _error;
};

class MacroTaskQueue {
  static void Dispose(MacroTaskQueue* queue) {
    uv_close(
        reinterpret_cast<uv_handle_t*>(&queue->idle_), [](uv_handle_t* handle) {
          MacroTaskQueue* queue = ContainerOf(
              &MacroTaskQueue::idle_, reinterpret_cast<uv_idle_t*>(handle));
          delete queue;
        });
    // TODO(chengzhong.wcz): proper close.
    uv_run(queue->loop_, UV_RUN_NOWAIT);
  }

 public:
  static std::shared_ptr<MacroTaskQueue> Create(uv_loop_t* loop,
                                                int max_tick_per_loop);
  void Enqueue(std::unique_ptr<MacroTask> task);

  inline int max_tick_per_loop() { return max_tick_per_loop_; }
  inline bool active() { return active_; }

 private:
  MacroTaskQueue(uv_loop_t* loop, int max_tick_per_loop);
  ~MacroTaskQueue();
  static void TickMacroTaskQueue(uv_idle_t* handle);
  void TickMacroTaskQueue();

  uv_loop_t* loop_;
  uv_idle_t idle_;
  int max_tick_per_loop_;
  bool active_;
  std::queue<std::unique_ptr<MacroTask>> queue_;
};

}  // namespace aworker
