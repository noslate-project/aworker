#include "aworker_perf.h"

#include "util.h"
#include "uv.h"

namespace aworker {
namespace per_process {
// https://w3c.github.io/hr-time/#dfn-time-origin
uint64_t time_origin = uv_hrtime();
// https://w3c.github.io/hr-time/#dfn-time-origin-timestamp
double time_origin_timestamp = GetCurrentTimeInMicroseconds();
}  // namespace per_process

uint64_t getTimeOrigin() {
  return per_process::time_origin;
}

double getTimeOriginTimestamp() {
  return per_process::time_origin_timestamp;
}

void RefreshTimeOrigin() {
  per_process::time_origin = uv_hrtime();
  per_process::time_origin_timestamp = GetCurrentTimeInMicroseconds();
}

}  // namespace aworker
