#include "macro_task_queue.h"
#include "debug_utils.h"

namespace aworker {

std::shared_ptr<MacroTaskQueue> MacroTaskQueue::Create(uv_loop_t* loop,
                                                       int max_tick_per_loop) {
  return std::shared_ptr<MacroTaskQueue>(
      new MacroTaskQueue(loop, max_tick_per_loop),
      FunctionDeleter<MacroTaskQueue, Dispose>());
}

MacroTaskQueue::MacroTaskQueue(uv_loop_t* loop, int max_tick_per_loop)
    : loop_(loop), max_tick_per_loop_(max_tick_per_loop), active_(false) {
  uv_idle_init(loop, &idle_);
}

MacroTaskQueue::~MacroTaskQueue() {}

void MacroTaskQueue::TickMacroTaskQueue(uv_idle_t* handle) {
  MacroTaskQueue* queue = ContainerOf(&MacroTaskQueue::idle_, handle);
  queue->TickMacroTaskQueue();
}

void MacroTaskQueue::TickMacroTaskQueue() {
  int processed = 0;

  per_process::Debug(DebugCategory::MACRO_TASK_QUEUE,
                     "process macro task queue, %zu\n",
                     queue_.size());
  while (processed < max_tick_per_loop_ && !queue_.empty()) {
    std::unique_ptr<MacroTask> task = std::move(queue_.front());
    queue_.pop();
    task->OnWorkTick();
    if (task->is_done() || task->has_error()) {
      per_process::Debug(DebugCategory::MACRO_TASK_QUEUE,
                         "macro task done with is_done: %d, has_error: %d\n",
                         task->is_done(),
                         task->has_error());
      task->OnDone();
      task.reset();
    } else {
      per_process::Debug(DebugCategory::MACRO_TASK_QUEUE,
                         "macro task re-queuing\n");
      queue_.push(std::move(task));
    }
    processed++;
  }
  per_process::Debug(
      DebugCategory::MACRO_TASK_QUEUE, "macro task processed %d\n", processed);

  if (queue_.empty()) {
    uv_idle_stop(&idle_);
    active_ = false;
    return;
  }
  per_process::Debug(DebugCategory::MACRO_TASK_QUEUE,
                     "macro task queue schedule next tick\n");
  active_ = true;
}

void MacroTaskQueue::Enqueue(std::unique_ptr<MacroTask> task) {
  per_process::Debug(DebugCategory::MACRO_TASK_QUEUE, "macro task enqueue\n");
  queue_.push(std::move(task));
  if (active_) {
    return;
  }
  CHECK_EQ(uv_idle_start(&idle_, TickMacroTaskQueue), 0);
  active_ = true;
}

}  // namespace aworker
