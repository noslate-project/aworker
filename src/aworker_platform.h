#ifndef SRC_AWORKER_PLATFORM_H_
#define SRC_AWORKER_PLATFORM_H_

/**
 * Design of Aworker Platform and it's tasks:
 *
 * + PlatformTaskRunner:
 *   * PostNonNestableTask -> `PostTask`
 *   * PostNonNestableDelayedTask -> `PostDelayedTask`
 *   * PostTask:
 *     1. Push the task to `task_runner._tasks`;
 *     2. Create the timer with `PlatformTaskRunner::ImmediateProcessor` if
 *        `task_runner._immediate_task_timer` is not set and then set it;
 *     3. At `PlatformTaskRunner::ImmediateProcessor`, swap
 * `task_runner._tasks`. Then go through the old list to run previously posted
 * tasks.
 *   * PostDelayedTask:
 *     1. Push this task to `task_runner._delayed_tasks` priority queue;
 *     2. Create the timer with `PlatformTaskRunner::ImmediateProcessor` if
 *        `task_runner._immediate_task_timer` is not set and then set it;
 *     3. At `PlatformTaskRunner::ImmediateProcessor`, drain deadline met
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

class PlatformTaskRunner : public v8::TaskRunner {
 public:
  static void ImmediateProcessor(uv_timer_t* timer);

 public:
  explicit PlatformTaskRunner(uv_loop_t* loop);
  virtual ~PlatformTaskRunner();

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
  static void Dispose(AworkerPlatform*);
  using AworkerPlatformPtr = DeleteFnPtr<AworkerPlatform, Dispose>;
  static AworkerPlatformPtr Create(uv_loop_t* loop);
  virtual ~AworkerPlatform();

  void CallOnWorkerThread(std::unique_ptr<v8::Task> task) override;
  void CallDelayedOnWorkerThread(std::unique_ptr<v8::Task> task,
                                 double delay) override;

  std::unique_ptr<v8::JobHandle> PostJob(
      v8::TaskPriority priority,
      std::unique_ptr<v8::JobTask> job_task) override;

  double MonotonicallyIncreasingTime() override;
  double CurrentClockTimeMillis() override;

  v8::TracingController* GetTracingController() override;

  inline int NumberOfWorkerThreads() override { return 0; }
  inline std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate) override {
    return task_runner_;
  }
  inline bool IdleTasksEnabled(v8::Isolate* isolate) override { return false; }

  void EvaluateCommandlineOptions(CommandlineParserGroup* cli);

  inline TraceAgent* trace_agent() { return trace_agent_.get(); }

  inline PlatformTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  explicit AworkerPlatform(uv_loop_t* loop);

 private:
  uv_loop_t* loop_;
  std::unique_ptr<TraceAgent> trace_agent_;
  std::unique_ptr<v8::TracingController::TraceStateObserver>
      trace_state_observer_;
  std::shared_ptr<PlatformTaskRunner> task_runner_;
};

}  // namespace aworker

#endif  // SRC_AWORKER_PLATFORM_H_
