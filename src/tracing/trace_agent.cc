#include "trace_agent.h"
#include "debug_utils.h"
#include "tracing/trace_event.h"
#include "tracing/trace_writer.h"

namespace aworker {

using v8::platform::tracing::TraceBuffer;
using v8::platform::tracing::TraceConfig;
using v8::platform::tracing::TraceObject;

constexpr uint64_t kFlushIntervalMs = 1000;

void TracingController::AddMetadataEvent(
    const unsigned char* category_group_enabled,
    const char* name,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const uint64_t* arg_values,
    std::unique_ptr<v8::ConvertableToTraceFormat>* convertable_values,
    unsigned int flags) {
  std::unique_ptr<TraceObject> trace_event = std::make_unique<TraceObject>();
  trace_event->Initialize(TRACE_EVENT_PHASE_METADATA,
                          category_group_enabled,
                          name,
                          aworker::tracing::kGlobalScope,  // scope
                          aworker::tracing::kNoId,         // id
                          aworker::tracing::kNoId,         // bind_id
                          num_args,
                          arg_names,
                          arg_types,
                          arg_values,
                          convertable_values,
                          TRACE_EVENT_FLAG_NONE,
                          CurrentTimestampMicroseconds(),
                          CurrentCpuTimestampMicroseconds());
  TraceAgent* agent = aworker::tracing::TraceEventHelper::GetTraceAgent();
  if (agent != nullptr) {
    agent->AddMetadataEvent(std::move(trace_event));
  }
}

TraceAgent::TraceAgent(uv_loop_t* loop) : loop_(loop) {
  CHECK_EQ(uv_timer_init(loop_, &flush_timer_), 0);
  tracing_controller_ = std::make_unique<TracingController>();
  tracing_controller_->Initialize(nullptr);
}

TraceAgent::~TraceAgent() {
  tracing_controller_->Initialize(nullptr);

  uv_close(reinterpret_cast<uv_handle_t*>(&flush_timer_), nullptr);
  uv_run(loop_, UV_RUN_NOWAIT);
}

void TraceAgent::SetLogDirectory(std::string log_directory) {
  /** ownership transfers to TraceBuffer */
  AworkerTraceWriter* trace_writer = new AworkerTraceWriter(
      this, loop_, SPrintF("%s/aworker_trace.%d", log_directory, getpid()));
  /** ownership transfers to TracingController */
  trace_buffer_ =
      TraceBuffer::CreateTraceBufferRingBuffer(kBufferChunks, trace_writer);
  tracing_controller_->Initialize(trace_buffer_);
}

void TraceAgent::Enable(const std::string& categories) {
  std::set<std::string> cat_set;
  std::string::size_type prev_pos = 0, pos = 0;

  while ((pos = categories.find(",", pos)) != std::string::npos) {
    cat_set.insert(categories.substr(prev_pos, pos - prev_pos));
    prev_pos = ++pos;
  }

  cat_set.insert(categories.substr(prev_pos, pos - prev_pos));  // Last word

  Enable(cat_set);
}

void TraceAgent::Enable(const std::set<std::string>& categories) {
  for (auto it : categories) {
    categories_.insert(it);
  }

  if (status_ != Status::kStopped) return;
  Start();
}

void TraceAgent::OnFlushInterval(uv_timer_t* timer) {
  TraceAgent* agent = ContainerOf(&TraceAgent::flush_timer_, timer);
  agent->trace_buffer_->Flush();
}

void TraceAgent::Start() {
  fprintf(stderr, "Trace agent started...\n");
  fflush(stderr);
  tracing_controller_->StartTracing(CreateTraceConfig().release());
  status_ = Status::kStarted;

  if (!timer_started_) {
    per_process::Debug(DebugCategory::TRACING, "Start trace flush interval\n");
    CHECK_EQ(uv_timer_start(&flush_timer_,
                            &TraceAgent::OnFlushInterval,
                            kFlushIntervalMs,
                            kFlushIntervalMs),
             0);
    uv_unref(reinterpret_cast<uv_handle_t*>(&flush_timer_));
    timer_started_ = true;
  }
}

void TraceAgent::Stop() {
  if (status_ != Status::kStarted) return;
  // Perform final Flush on TraceBuffer. We don't want the tracing controller
  // to flush the buffer again on destruction of the V8::Platform.
  tracing_controller_->StopTracing();
  status_ = Status::kStopped;
}

void TraceAgent::AddMetadataEvent(
    std::unique_ptr<v8::platform::tracing::TraceObject> metadata) {
  metadataes_.push_back(std::move(metadata));
}

void TraceAgent::RequestMetadataEvents(AworkerTraceWriter* writer) {
  for (auto it = metadataes_.cbegin(); it != metadataes_.cend(); it++) {
    writer->AppendTraceEvent((*it).get());
  }
}

std::unique_ptr<TraceConfig> TraceAgent::CreateTraceConfig() const {
  std::unique_ptr<TraceConfig> trace_config = std::make_unique<TraceConfig>();
  for (auto it : categories_) {
    trace_config->AddIncludedCategory(it.c_str());
  }
  return trace_config;
}

}  // namespace aworker
