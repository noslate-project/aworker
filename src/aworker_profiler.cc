#include "aworker_profiler.h"
#include <sys/time.h>
#include <fstream>
#include <sstream>
#include "diag_report.h"
#include "json_utils.h"
#include "uv.h"
#include "v8-profiler.h"

namespace aworker {
namespace profiler {

void SerializeNode(v8::Isolate* isolate,
                   const v8::CpuProfileNode* node,
                   JSONWriter* writer) {
  v8::HandleScope scope(isolate);
  v8::String::Utf8Value funcion_name(isolate, node->GetFunctionName());
  v8::String::Utf8Value url(isolate, node->GetScriptResourceName());

  // set node
  writer->json_start();
  writer->json_keyvalue("id", node->GetNodeId());
  writer->json_keyvalue("hitCount", node->GetHitCount());
  // set call frame
  writer->json_objectstart("callFrame");
  writer->json_keyvalue("functionName", *funcion_name);
  writer->json_keyvalue("scriptId", node->GetScriptId());
  writer->json_keyvalue("bailoutReason", node->GetBailoutReason());
  writer->json_keyvalue("url", *url);
  writer->json_keyvalue("lineNumber", node->GetLineNumber());
  writer->json_keyvalue("columnNumber", node->GetColumnNumber());
  writer->json_objectend();

  // set children
  int count = node->GetChildrenCount();
  writer->json_arraystart("children");
  for (int index = 0; index < count; index++) {
    writer->json_element(node->GetChild(index)->GetNodeId());
  }
  writer->json_arrayend();
  writer->json_end();

  for (int index = 0; index < count; index++) {
    SerializeNode(isolate, node->GetChild(index), writer);
  }
}

void Serialize(v8::Isolate* isolate,
               v8::CpuProfile* node,
               std::string filename) {
  std::ofstream outfile;
  outfile.open(filename, std::ios::out | std::ios::binary);
  if (!outfile.is_open()) {
    return;
  }
  // record cpu profile
  JSONWriter writer(outfile, false);
  writer.json_start();
  writer.json_keyvalue("typeId", "aworker-cpu-profile");

  // set title
  v8::String::Utf8Value title(isolate, node->GetTitle());
  writer.json_keyvalue("title", *title);

  // set nodes
  writer.json_arraystart("nodes");
  SerializeNode(isolate, node->GetTopDownRoot(), &writer);
  writer.json_arrayend();

  // set start/end time
  writer.json_keyvalue("startTime", node->GetStartTime());
  writer.json_keyvalue("endTime", node->GetEndTime());

  // set samples
  int count = node->GetSamplesCount();
  writer.json_arraystart("samples");
  for (int index = 0; index < count; ++index) {
    writer.json_element(node->GetSample(index)->GetNodeId());
  }
  writer.json_arrayend();

  // set timestamps
  writer.json_arraystart("timeDeltas");
  for (int index = 0; index < count; ++index) {
    int64_t prev =
        index == 0 ? node->GetStartTime() : node->GetSampleTimestamp(index - 1);
    int64_t delta = node->GetSampleTimestamp(index) - prev;
    writer.json_element(delta);
  }
  writer.json_arrayend();

  // write to file
  writer.json_end();
}

static v8::CpuProfiler* cpu_profiler;

void StartCpuProfiler(v8::Isolate* isolate, std::string t) {
  if (cpu_profiler) {
    return;
  }
  cpu_profiler = v8::CpuProfiler::New(isolate);
  v8::Local<v8::String> title =
      v8::String::NewFromUtf8(isolate, t.c_str(), v8::NewStringType::kNormal)
          .ToLocalChecked();
  cpu_profiler->StartProfiling(title, true);
}

void StopCpuProfiler(v8::Isolate* isolate, std::string t) {
  if (!cpu_profiler) {
    return;
  }
  v8::Local<v8::String> title =
      v8::String::NewFromUtf8(isolate, t.c_str(), v8::NewStringType::kNormal)
          .ToLocalChecked();
  v8::CpuProfile* profile = cpu_profiler->StopProfiling(title);

  Serialize(isolate,
            profile,
            *report::DiagnosticFilename("CPU", "cpuprofile"));

  profile->Delete();
  cpu_profiler->Dispose();
  cpu_profiler = nullptr;
}

}  // namespace profiler
}  // namespace aworker
