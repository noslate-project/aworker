#pragma once
#include <memory>
#include <set>

#include "libplatform/v8-tracing.h"
#include "util.h"
#include "uv.h"

namespace aworker {

class AworkerTraceWriter;

class TracingController : public v8::platform::tracing::TracingController {
 public:
  TracingController() : v8::platform::tracing::TracingController() {}

  int64_t CurrentTimestampMicroseconds() override { return uv_hrtime() / 1000; }

  void AddMetadataEvent(
      const unsigned char* category_group_enabled,
      const char* name,
      int num_args,
      const char** arg_names,
      const unsigned char* arg_types,
      const uint64_t* arg_values,
      std::unique_ptr<v8::ConvertableToTraceFormat>* convertable_values,
      unsigned int flags);
};

// Same-thread trace agent. Manages trace controllers.
// TODO(chengzhong.wcz): multiple trace client support (inspector, etc.).
class TraceAgent {
  using TraceBuffer = v8::platform::tracing::TraceBuffer;
  using TraceConfig = v8::platform::tracing::TraceConfig;
  using TraceObject = v8::platform::tracing::TraceObject;

  enum class Status {
    kStopped,
    kStarted,
    kSuspended,
  };

 public:
  explicit TraceAgent(uv_loop_t* loop);
  ~TraceAgent();

  void SetLogDirectory(std::string log_directory);

  TracingController* GetTracingController() {
    TracingController* controller = tracing_controller_.get();
    CHECK_NOT_NULL(controller);
    return controller;
  }

  // Enable with comma separated string;
  void Enable(const std::string& categories);
  void Enable(const std::set<std::string>& categories);
  void Stop();

  void AddMetadataEvent(std::unique_ptr<TraceObject> metadata);
  void RequestMetadataEvents(AworkerTraceWriter* writer);

  class SuspendTracingScope {
   public:
    explicit SuspendTracingScope(TraceAgent* agent) : agent_(agent) {
      agent_->status_ = Status::kSuspended;
      agent_->tracing_controller_->StopTracing();
    }

    ~SuspendTracingScope() {
      CHECK_EQ(agent_->status_, Status::kSuspended);
      if (agent_->categories_.size() > 0) {
        agent_->Start();
      } else {
        agent_->status_ = Status::kStopped;
      }
    }

   private:
    TraceAgent* agent_;
  };

 private:
  void Start();
  static void OnFlushInterval(uv_timer_t* handle);

  static const size_t kBufferChunks = 1024;
  std::unique_ptr<TraceConfig> CreateTraceConfig() const;

  std::set<std::string> categories_;
  Status status_ = Status::kStopped;
  uv_loop_t* loop_;
  std::unique_ptr<TracingController> tracing_controller_;
  TraceBuffer* trace_buffer_;

  std::vector<std::unique_ptr<TraceObject>> metadataes_;

  uv_timer_t flush_timer_;
  bool timer_started_ = false;
};

}  // namespace aworker
