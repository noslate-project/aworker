#pragma once
#include <queue>
#include <sstream>
#include "libplatform/v8-tracing.h"
#include "uv.h"
#include "zero_copy_file_stream.h"

namespace aworker {

class TraceAgent;
class AworkerTraceWriter : public v8::platform::tracing::TraceWriter {
 public:
  AworkerTraceWriter(TraceAgent* agent,
                     uv_loop_t* loop,
                     const std::string& log_file_prefix);
  ~AworkerTraceWriter();
  void AppendTraceEvent(
      v8::platform::tracing::TraceObject* trace_event) override;
  void Flush() override;
  void FlushOnDetroy();

 private:
  static const int kTracesPerFile = 1 << 19;

  void OpenNewFileForStreaming();
  void WriteToFile(std::string&& str);

  std::size_t total_traces_ = 0;
  std::size_t file_num_ = 0;

  TraceAgent* agent_;
  uv_loop_t* loop_;
  std::string log_file_prefix_;
  std::ostringstream stream_;
  std::unique_ptr<TraceWriter> json_trace_writer_;
  bool metadata_written_ = false;
  UvZeroCopyOutputFileStream::UvZeroCopyOutputFileStreamPtr fs_;
  bool flush_completed_ = true;
};

}  // namespace aworker
