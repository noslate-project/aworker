#include "aworker_perf.h"
#include <cmath>
#include "aworker_binding.h"
#include "immortal.h"
#include "uv.h"

using v8::BigInt;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Number;
using v8::Value;

namespace aworker {

// Microseconds in a millisecond, as a float.
#define MICROS_PER_MILLIS 1e3
#define NANOS_PER_MILLIS 1e6

// TODO(chengzhong.wcz): v8 fast api call
AWORKER_METHOD(Hrtime) {
  Isolate* isolate = info.GetIsolate();
  uint64_t time = uv_hrtime();
  info.GetReturnValue().Set(1.0 * time / NANOS_PER_MILLIS);
}

// Return idle time of the event loop
void LoopIdleTime(const FunctionCallbackInfo<Value>& info) {
  Immortal* immortal = Immortal::GetCurrent(info);
  uint64_t idle_time = uv_metrics_idle_time(immortal->event_loop());
  info.GetReturnValue().Set(1.0 * idle_time / NANOS_PER_MILLIS);
}

AWORKER_METHOD(GetTimeOrigin) {
  Isolate* isolate = info.GetIsolate();
  info.GetReturnValue().Set(1.0 * getTimeOrigin() / NANOS_PER_MILLIS);
}

AWORKER_METHOD(GetTimeOriginTimeStamp) {
  info.GetReturnValue().Set(Number::New(
      info.GetIsolate(), getTimeOriginTimestamp() / MICROS_PER_MILLIS));
}

AWORKER_METHOD(GetTimeOriginRelativeTimeStamp) {
  info.GetReturnValue().Set(
      Number::New(info.GetIsolate(),
                  (GetCurrentTimeInMicroseconds() - getTimeOriginTimestamp()) /
                      MICROS_PER_MILLIS));
}

AWORKER_BINDING(Init) {
  immortal->SetFunctionProperty(exports, "hrtime", Hrtime);
  immortal->SetFunctionProperty(exports, "loopIdleTime", LoopIdleTime);
  immortal->SetFunctionProperty(exports, "getTimeOrigin", GetTimeOrigin);
  immortal->SetFunctionProperty(
      exports, "getTimeOriginTimeStamp", GetTimeOriginTimeStamp);
  immortal->SetFunctionProperty(exports,
                                "getTimeOriginRelativeTimeStamp",
                                GetTimeOriginRelativeTimeStamp);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  registry->Register(Hrtime);
  registry->Register(LoopIdleTime);
  registry->Register(GetTimeOrigin);
  registry->Register(GetTimeOriginTimeStamp);
  registry->Register(GetTimeOriginRelativeTimeStamp);
}

}  // namespace aworker

AWORKER_BINDING_REGISTER(perf, aworker::Init, aworker::Init)
