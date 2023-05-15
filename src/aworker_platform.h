#ifndef SRC_AWORKER_PLATFORM_H_
#define SRC_AWORKER_PLATFORM_H_

/**
 * Design of Aworker Platform and it's tasks:
 *
 * + SameThreadTaskRunner:
 *   * PostNonNestableTask -> `PostTask`
 *   * PostNonNestableDelayedTask -> `PostDelayedTask`
 *   * PostTask:
 *     1. Push the task to `task_runner._tasks`;
 *     2. Create the timer with `SameThreadTaskRunner::ImmediateProcessor` if
 *        `task_runner._immediate_task_timer` is not set and then set it;
 *     3. At `SameThreadTaskRunner::ImmediateProcessor`, swap
 * `task_runner._tasks`. Then go through the old list to run previously posted
 * tasks.
 *   * PostDelayedTask:
 *     1. Push this task to `task_runner._delayed_tasks` priority queue;
 *     2. Create the timer with `SameThreadTaskRunner::ImmediateProcessor` if
 *        `task_runner._immediate_task_timer` is not set and then set it;
 *     3. At `SameThreadTaskRunner::ImmediateProcessor`, drain deadline met
 * delayed tasks queue items.
 */

#include <list>
#include <queue>

#include "command_parser.h"
#include "libplatform/libplatform.h"
#include "tracing/trace_agent.h"
#include "uv.h"
#include "v8.h"

namespace aworker {

class PlatformTask {
 public:
  explicit PlatformTask(std::unique_ptr<v8::Task> task) : task_(move(task)) {}

  inline void Run() { task_->Run(); }

 private:
  std::unique_ptr<v8::Task> task_;
};

class PlatformDelayedTask : public PlatformTask {
 public:
  PlatformDelayedTask(std::unique_ptr<v8::Task> task, uint64_t deadline)
      : PlatformTask(move(task)), deadline_(deadline) {}
  inline uint64_t deadline() { return deadline_; }

 private:
  uint64_t deadline_;
};

struct PlatformDelayedTaskCompare {
  bool operator()(const std::unique_ptr<PlatformDelayedTask>& lhs,
                  const std::unique_ptr<PlatformDelayedTask>& rhs) const {
    return lhs->deadline() > rhs->deadline();
  }
};

class SameThreadTaskRunner : public v8::TaskRunner {
 public:
  static void ImmediateProcessor(uv_timer_t* timer);

 public:
  explicit SameThreadTaskRunner(uv_loop_t* loop);
  virtual ~SameThreadTaskRunner();

  void PostTask(std::unique_ptr<v8::Task> task) override;
  void PostNonNestableTask(std::unique_ptr<v8::Task> task) override;
  void PostDelayedTask(std::unique_ptr<v8::Task> task, double delay) override;
  void PostNonNestableDelayedTask(std::unique_ptr<v8::Task> task,
                                  double delay) override;
  inline void PostIdleTask(std::unique_ptr<v8::IdleTask> task) override {
    UNREACHABLE();
  };
  inline bool IdleTasksEnabled() override { return false; };

  inline bool NonNestableTasksEnabled() const override { return true; }
  inline bool NonNestableDelayedTasksEnabled() const override { return true; }

 public:
  void ResetImmediateTaskTimer(int64_t timer);
  int64_t PostImmediateTimer(uint64_t timeout = 0);

  void DrainTasks();

 private:
  using DelayedTasksPtr = std::unique_ptr<PlatformDelayedTask>;
  using DelayedTaskQueue = std::priority_queue<DelayedTasksPtr,
                                               std::vector<DelayedTasksPtr>,
                                               PlatformDelayedTaskCompare>;
  uv_loop_t* loop_;
  uv_timer_t* timer_;
  DelayedTaskQueue delayed_tasks_;
  std::list<std::unique_ptr<PlatformTask>> tasks_;
  uint64_t next_deadline_;
};

class AworkerPlatform : public v8::Platform {
 public:
  enum ThreadMode {
    kSingleThread,
    kMultiThread,
  };

  explicit AworkerPlatform(ThreadMode thread_mode);
  AworkerPlatform(const AworkerPlatform&) = delete;
  ~AworkerPlatform() noexcept override;

  class Scope {
   public:
    explicit Scope(AworkerPlatform* platform);
    ~Scope();

    Scope(const Scope&) = delete;
  };

  void CallOnWorkerThread(std::unique_ptr<v8::Task> task) override;
  void CallDelayedOnWorkerThread(std::unique_ptr<v8::Task> task,
                                 double delay) override;

  std::unique_ptr<v8::JobHandle> PostJob(
      v8::TaskPriority priority,
      std::unique_ptr<v8::JobTask> job_task) override;

  double MonotonicallyIncreasingTime() override;
  double CurrentClockTimeMillis() override;

  v8::TracingController* GetTracingController() override;

  int NumberOfWorkerThreads() override;
  inline std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate) override {
    return task_runner_;
  }
  inline bool IdleTasksEnabled(v8::Isolate* isolate) override { return false; }

  void EvaluateCommandlineOptions(CommandlineParserGroup* cli);

  inline TraceAgent* trace_agent() { return trace_agent_.get(); }

  inline SameThreadTaskRunner* task_runner() { return task_runner_.get(); }

  inline uv_loop_t* loop() { return &loop_; }

  inline std::shared_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator() {
    return array_buffer_allocator_;
  }

 private:
  ThreadMode thread_mode_;
  uv_loop_t loop_;
  std::unique_ptr<TraceAgent> trace_agent_;
  std::unique_ptr<v8::TracingController::TraceStateObserver>
      trace_state_observer_;
  std::shared_ptr<SameThreadTaskRunner> task_runner_;
  std::unique_ptr<v8::Platform> worker_thread_platform_;
  std::shared_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator_;
};

}  // namespace aworker

#endif  // SRC_AWORKER_PLATFORM_H_
