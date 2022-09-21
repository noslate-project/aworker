#ifndef SRC_AWORKER_PERF_H_
#define SRC_AWORKER_PERF_H_
#include <stdint.h>

namespace aworker {

uint64_t getTimeOrigin();
double getTimeOriginTimestamp();
void RefreshTimeOrigin();

}  // namespace aworker

#endif  // SRC_AWORKER_PERF_H_
