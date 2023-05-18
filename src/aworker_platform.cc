#include <cmath>

#include "aworker_platform.h"
#include "command_parser.h"
#include "debug_utils.h"
#include "immortal.h"
#include "metadata.h"
#include "tracing/trace_event.h"
#include "tracing/traced_value.h"
#include "util.h"
#include "uv.h"

#ifdef __APPLE__
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif

namespace aworker {
using std::move;
using std::shared_ptr;
using std::unique_ptr;
using v8::JobHandle;
using v8::JobTask;
using v8::TaskPriority;
using v8::platform::tracing::TraceObject;

namespace {
// in ms;
inline uint64_t now_in_ms() {
  return uv_hrtime() / 1000 / 1000;
}
}  // namespace

// Ensures that __metadata trace events are only emitted
// when tracing is enabled.
class AworkerTraceStateObserver
    : public v8::TracingController::TraceStateObserver {
 public:
  inline void OnTraceEnabled() override {
    TRACE_EVENT_METADATA1("__metadata", "process_name", "name", "aworker");
    TRACE_EVENT_METADATA1(
        "__metadata", "thread_name", "name", "JavaScriptMainThread");
    TRACE_EVENT_METADATA1("__metadata",
                          "version",
                          "aworker",
                          per_process::metadata.aworker.c_str());

    auto trace_process = tracing::TracedValue::Create();
    trace_process->BeginDictionary("versions");

#define V(key)                                                                 \
  trace_process->SetString(#key, per_process::metadata.key.c_str());
    AWORKER_VERSIONS_KEYS(V)
#undef V

    trace_process->EndDictionary();

    trace_process->SetString("arch", per_process::metadata.arch.c_str());
    trace_process->SetString("platform",
                             per_process::metadata.platform.c_str());

    TRACE_EVENT_METADATA1(
        "__metadata", "aworker", "process", std::move(trace_process));

    // This only runs the first time tracing is enabled
    controller_->RemoveTraceStateObserver(this);
  }

  inline void OnTraceDisabled() override {
    // Do nothing here. This should never be called because the
    // observer removes itself when OnTraceEnabled() is called.
    UNREACHABLE();
  }

  explicit AworkerTraceStateObserver(v8::TracingController* controller)
      : controller_(controller) {}
  ~AworkerTraceStateObserver() override = default;

 private:
  v8::TracingController* controller_;
};

// static
std::shared_ptr<ForegroundTaskRunner> ForegroundTaskRunner::New(
    uv_loop_t* loop) {
  return std::shared_ptr<ForegroundTaskRunner>(new ForegroundTaskRunner(loop),
                                               Delete);
}

// static
void ForegroundTaskRunner::TimerProcessor(uv_timer_t* timer) {
  ForegroundTaskRunner* runner =
      ContainerOf(&ForegroundTaskRunner::timer_, timer);
  runner->RunTasks();
}

// static
void ForegroundTaskRunner::ImmediateProcessor(uv_async_t* async) {
  ForegroundTaskRunner* runner =
      ContainerOf(&ForegroundTaskRunner::async_, async);
  runner->RunTasks();
}

void ForegroundTaskRunner::RunTasks() {
  DrainTasks();

  uint64_t now = now_in_ms();
  uint64_t next_timeout = 0;
  {
    std::scoped_lock guard(queue_mutex_);
    if (delayed_tasks_.size() > 0) {
      next_timeout =
          std::max(delayed_tasks_.top()->deadline() - now, uint64_t(1));
    }
  }
  if (next_timeout == 0) {
    uv_timer_stop(&timer_);
    return;
  }
  per_process::Debug(
      DebugCategory::PLATFORM, "scheduling next immediate: %d\n", next_timeout);
  uv_timer_start(&timer_, TimerProcessor, next_timeout, 0);
}

void ForegroundTaskRunner::DrainTasks() {
  uint64_t now = now_in_ms();
  std::list<std::unique_ptr<PlatformTask>> tasks;
  std::list<std::unique_ptr<PlatformDelayedTask>> delayed_tasks;
  {
    std::scoped_lock guard(queue_mutex_);
    tasks_.swap(tasks);
    while (delayed_tasks_.size() > 0) {
      auto& it = const_cast<std::unique_ptr<PlatformDelayedTask>&>(
          delayed_tasks_.top());
      if (it->deadline() > now) {
        break;
      }
      delayed_tasks.push_back(std::move(it));
      delayed_tasks_.pop();
    }
  }
  for (auto& it : tasks) {
    it->Run();
  }
  for (auto& it : delayed_tasks) {
    it->Run();
  }
}

ForegroundTaskRunner::ForegroundTaskRunner(uv_loop_t* loop) {
  uv_timer_init(loop, &timer_);
  uv_async_init(loop, &async_, ImmediateProcessor);
  uv_unref(reinterpret_cast<uv_handle_t*>(&timer_));
  uv_unref(reinterpret_cast<uv_handle_t*>(&async_));
}

ForegroundTaskRunner::~ForegroundTaskRunner() {}

void ForegroundTaskRunner::Delete(ForegroundTaskRunner* runner) {
  uv_close(reinterpret_cast<uv_handle_t*>(&runner->timer_), nullptr);
  uv_close(reinterpret_cast<uv_handle_t*>(&runner->async_),
           [](uv_handle_t* handle) {
             ForegroundTaskRunner* runner =
                 ContainerOf(&ForegroundTaskRunner::async_,
                             reinterpret_cast<uv_async_t*>(handle));
             delete runner;
           });
}

void ForegroundTaskRunner::PostTask(unique_ptr<v8::Task> task) {
  {
    std::scoped_lock guard(queue_mutex_);
    tasks_.push_back(std::make_unique<PlatformTask>(move(task)));
  }
  uv_async_send(&async_);
}

void ForegroundTaskRunner::PostDelayedTask(unique_ptr<v8::Task> task,
                                           double delay) {
  uint64_t deadline = now_in_ms() + std::llround(delay * 1000);
  {
    std::scoped_lock guard(queue_mutex_);
    delayed_tasks_.push(
        std::make_unique<PlatformDelayedTask>(move(task), deadline));
  }
  uv_async_send(&async_);
}

void ForegroundTaskRunner::PostNonNestableTask(unique_ptr<v8::Task> task) {
  PostTask(move(task));
}

void ForegroundTaskRunner::PostNonNestableDelayedTask(unique_ptr<v8::Task> task,
                                                      double delay) {
  PostDelayedTask(move(task), delay);
}

AworkerPlatform::AworkerPlatform(ThreadMode thread_mode, int thread_pool_size)
    : thread_mode_(thread_mode) {
  CHECK_EQ(uv_loop_init(&loop_), 0);
  CHECK_EQ(uv_loop_configure(&loop_, UV_METRICS_IDLE_TIME), 0);
  task_runner_ = ForegroundTaskRunner::New(&loop_);
  trace_agent_ = std::make_unique<TraceAgent>(&loop_);
  aworker::TracingController* controller = trace_agent_->GetTracingController();
  trace_state_observer_ =
      std::make_unique<AworkerTraceStateObserver>(controller);
  controller->AddTraceStateObserver(trace_state_observer_.get());

  array_buffer_allocator_ = std::shared_ptr<v8::ArrayBuffer::Allocator>(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());

  if (thread_mode == kMultiThread) {
    // TODO(chengzhong.wcz): Proper worker thread task runner support.
    worker_thread_platform_ =
        v8::platform::NewDefaultPlatform(thread_pool_size);
  }
}

AworkerPlatform::~AworkerPlatform() {
  trace_agent_->Stop();
  // wait until trace agent flushed
  while (uv_loop_alive(&loop_) != 0) {
    uv_run(&loop_, UV_RUN_DEFAULT);
  }

  task_runner_.reset();
  trace_agent_.reset();
  CheckedUvLoopClose(&loop_);
}

int AworkerPlatform::NumberOfWorkerThreads() {
  if (thread_mode_) {
    return worker_thread_platform_->NumberOfWorkerThreads();
  }
  return 0;
}

void AworkerPlatform::CallOnWorkerThread(unique_ptr<v8::Task> task) {
  CHECK_EQ(thread_mode_, kMultiThread);
  CHECK_NOT_NULL(worker_thread_platform_);
  worker_thread_platform_->CallOnWorkerThread(std::move(task));
}

void AworkerPlatform::CallDelayedOnWorkerThread(unique_ptr<v8::Task> task,
                                                double delay) {
  CHECK_EQ(thread_mode_, kMultiThread);
  CHECK_NOT_NULL(worker_thread_platform_);
  worker_thread_platform_->CallDelayedOnWorkerThread(std::move(task), delay);
}

std::unique_ptr<JobHandle> AworkerPlatform::PostJob(
    TaskPriority priority, std::unique_ptr<JobTask> job_task) {
  return v8::platform::NewDefaultJobHandle(
      this, priority, std::move(job_task), NumberOfWorkerThreads());
}

double AworkerPlatform::MonotonicallyIncreasingTime() {
#ifdef __APPLE__
  static bool inited = false;
  static uint64_t (*time_func)(void);
  static mach_timebase_info_data_t timebase;

  if (!inited) {
    CHECK_EQ(mach_timebase_info(&timebase), KERN_SUCCESS);
    time_func = (uint64_t(*)(void))dlsym(RTLD_DEFAULT, "mach_continuous_time");
    if (time_func == nullptr) time_func = mach_absolute_time;
    inited = true;
  }

  return (time_func() * timebase.numer / timebase.denom) / 1e9;  // NOLINT
#else
  struct timespec t;
  clock_t clock_id;

  clock_id = CLOCK_MONOTONIC;

  if (clock_gettime(clock_id, &t)) return 0; /* Not really possible. */

  return (t.tv_sec * (uint64_t)1e9 + t.tv_nsec) / 1e9;
#endif
}

double AworkerPlatform::CurrentClockTimeMillis() {
  return v8::Platform::SystemClockTimeMillis();
}

v8::TracingController* AworkerPlatform::GetTracingController() {
  return trace_agent_->GetTracingController();
}

void AworkerPlatform::EvaluateCommandlineOptions(CommandlineParserGroup* cli) {
  std::string trace_event_directory =
      cli->has_trace_event_directory() ? cli->trace_event_directory() : cwd();
  trace_agent_->SetLogDirectory(trace_event_directory);
  if (cli->enable_trace_event() && cli->has_trace_event_categories()) {
    trace_agent_->Enable(cli->trace_event_categories());
  }
}

AworkerPlatform::Scope::Scope(AworkerPlatform* platform) {
  v8::V8::InitializePlatform(platform);
  aworker::tracing::TraceEventHelper::SetTraceAgent(platform->trace_agent());
}

AworkerPlatform::Scope::~Scope() {
  aworker::tracing::TraceEventHelper::SetTraceAgent(nullptr);
  v8::V8::ShutdownPlatform();
}

}  // namespace aworker
