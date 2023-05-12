#ifndef SRC_AWORKER_PERF_H_
#define SRC_AWORKER_PERF_H_
#include <stdint.h>
#include "util.h"

namespace aworker {

uint64_t getTimeOrigin();
double getTimeOriginTimestamp();
void RefreshTimeOrigin();

#define AWORKER_PERFORMANCE_MILESTONES(V)                                      \
  V(AWORKER_START, "aworker_start")                                            \
  V(LOOP_START, "loop_start")                                                  \
  V(BOOTSTRAP_AGENT, "bootstrap_agent")                                        \
  V(READ_SCRIPT_FILE, "read_script_file")                                      \
  V(PARSE_SCRIPT, "parse_script")                                              \
  V(EVALUATE_SCRIPT, "evaluate_script")                                        \
  V(EVALUATE, "evaluate")                                                      \
  V(AFTER_FORK, "after_fork")

enum PerformanceMilestone {
#define V(name, _) AWORKER_PERFORMANCE_MILESTONE_##name,
  AWORKER_PERFORMANCE_MILESTONES(V)
#undef V
      AWORKER_PERFORMANCE_MILESTONE_INVALID
};

extern const uint64_t performance_process_start;
extern const double performance_process_start_timestamp;
extern uint64_t performance_v8_start;
namespace per_process {
extern double milestones[AWORKER_PERFORMANCE_MILESTONE_INVALID];
}

void RefreshMilestones();
void Mark(enum PerformanceMilestone milestone,
          double ts = GetCurrentTimeInMicroseconds());

}  // namespace aworker

#endif  // SRC_AWORKER_PERF_H_
