#ifndef SRC_AWORKER_PROFILER_H_
#define SRC_AWORKER_PROFILER_H_
#include <iostream>
#include "v8.h"
namespace aworker {
namespace profiler {
void StartCpuProfiler(v8::Isolate* isolate, std::string t);
void StopCpuProfiler(v8::Isolate* isolate, std::string t);
}  // namespace profiler

}  // namespace aworker

#endif  // SRC_AWORKER_PROFILER_H_
