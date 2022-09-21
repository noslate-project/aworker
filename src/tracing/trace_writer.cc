#include "trace_writer.h"
#include "debug_utils.h"
#include "tracing/trace_agent.h"

namespace aworker {

AworkerTraceWriter::AworkerTraceWriter(TraceAgent* agent,
                                       uv_loop_t* loop,
                                       const std::string& log_file_prefix)
    : agent_(agent), loop_(loop), log_file_prefix_(log_file_prefix) {}

AworkerTraceWriter::~AworkerTraceWriter() {
  FlushOnDetroy();
  while (!flush_completed_ && uv_loop_alive(loop_) != 0) {
    uv_run(loop_, UV_RUN_ONCE);
  }
  CHECK_EQ(flush_completed_, true);
}

void AworkerTraceWriter::AppendTraceEvent(
    v8::platform::tracing::TraceObject* trace_event) {
  // If this is the first trace event, open a new file for streaming.
  if (total_traces_ == 0) {
    OpenNewFileForStreaming();
    // Constructing a new JSONTraceWriter object appends "{\"traceEvents\":["
    // to stream_.
    // In other words, the constructor initializes the serialization stream
    // to a state where we can start writing trace events to it.
    // Repeatedly constructing and destroying json_trace_writer_ allows
    // us to use V8's JSON writer instead of implementing our own.
    json_trace_writer_.reset(TraceWriter::CreateJSONTraceWriter(stream_));
    metadata_written_ = false;
  }
  ++total_traces_;
  json_trace_writer_->AppendTraceEvent(trace_event);
}

void AworkerTraceWriter::FlushOnDetroy() {
  if (total_traces_ == 0) {
    return;
  }
  if (!metadata_written_) {
    metadata_written_ = true;
    agent_->RequestMetadataEvents(this);
  }
  // Destroying the member JSONTraceWriter object appends "]}" to
  // stream_ - in other words, ending a JSON file.
  json_trace_writer_.reset();
  std::string str = stream_.str();
  stream_.str("");
  stream_.clear();
  WriteToFile(std::move(str));
  // TODO(chengzhong.wcz): improve writable stream lifetime management.
  fs_.reset();
}

void AworkerTraceWriter::Flush() {
  if (total_traces_ == 0) {
    return;
  }
  per_process::Debug(DebugCategory::TRACING, "flush writer\n");
  if (!metadata_written_) {
    metadata_written_ = true;
    agent_->RequestMetadataEvents(this);
  }
  if (total_traces_ >= kTracesPerFile) {
    total_traces_ = 0;
    // Destroying the member JSONTraceWriter object appends "]}" to
    // stream_ - in other words, ending a JSON file.
    json_trace_writer_.reset();
  }
  std::string str = stream_.str();
  stream_.str("");
  stream_.clear();
  WriteToFile(std::move(str));
}

void AworkerTraceWriter::OpenNewFileForStreaming() {
  fs_.reset();

  ++file_num_;
  flush_completed_ = false;
  std::string filepath = SPrintF("%s.%d.log", log_file_prefix_, file_num_);
  per_process::Debug(
      DebugCategory::TRACING, "open new file for streaming: %s\n", filepath);
  fs_ = UvZeroCopyOutputFileStream::Create(
      loop_, filepath, [this](UvZeroCopyOutputFileStream*) {
        flush_completed_ = true;
      });
}

void AworkerTraceWriter::WriteToFile(std::string&& str) {
  void* data;
  int size;
  size_t remaining = str.length();

  // TODO(chengzhong.wcz): Proper fs abstraction; do not copy :(
  while (remaining > 0) {
    CHECK(fs_->Next(&data, &size));
    size_t to_be_written =
        remaining > static_cast<size_t>(size) ? size : remaining;
    memcpy(data, str.c_str() + str.length() - remaining, to_be_written);
    remaining = remaining - to_be_written;
    if (to_be_written < static_cast<size_t>(size)) {
      fs_->BackUp(size - to_be_written);
    }
  }
}

}  // namespace aworker
