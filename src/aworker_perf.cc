#include "aworker_perf.h"

#include "uv.h"

namespace aworker {
namespace per_process {
double milestones[AWORKER_PERFORMANCE_MILESTONE_INVALID];
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

void RefreshMilestones() {
  for (size_t i = 0; i < AWORKER_PERFORMANCE_MILESTONE_INVALID; i++) {
    per_process::milestones[i] = 0;
  }
}

void Mark(PerformanceMilestone milestone, double ts) {
  per_process::milestones[milestone] = ts;
}

}  // namespace aworker
