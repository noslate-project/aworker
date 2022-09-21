#ifndef SRC_AWORKER_V8_H_
#define SRC_AWORKER_V8_H_
#include "v8.h"

namespace aworker_v8 {

void ResetRandomNumberGenerator(v8::Isolate* isolate,
                                v8::Local<v8::Context> context,
                                int64_t seed);
void ReconfigureHeapAfterResetV8Options(v8::Isolate* isolate);

}  // namespace aworker_v8

#endif  // SRC_AWORKER_V8_H_
