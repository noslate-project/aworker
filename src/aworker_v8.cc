#include "aworker_v8.h"

#include "src/execution/isolate.h"
#include "src/numbers/math-random.h"
#include "src/objects/contexts-inl.h"

namespace aworker_v8 {

namespace i = v8::internal;

void ResetRandomNumberGenerator(v8::Isolate* isolate,
                                v8::Local<v8::Context> context,
                                int64_t seed) {
  i::Isolate* i = reinterpret_cast<i::Isolate*>(isolate);
  i->random_number_generator()->SetSeed(seed);
  i::Address c = *reinterpret_cast<const i::Address*>(*context);
  i::Context native_context = i::Context::cast(i::Object(c));
  i::MathRandom::ResetContext(native_context);
  i::MathRandom::RefillCache(i, c);
}

// TODO(kaidi.zkd): How to unit test?
void ReconfigureHeapAfterResetV8Options(v8::Isolate* isolate) {
  i::Isolate* i = reinterpret_cast<i::Isolate*>(isolate);
  v8::ResourceConstraints constraints;
  i->heap()->ConfigureHeap(constraints);
}

}  // namespace aworker_v8
