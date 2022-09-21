#include "aworker_perf.h"
#include <cmath>
#include "aworker_binding.h"
#include "immortal.h"
#include "uv.h"

using v8::BigInt;
using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Number;

namespace aworker {

// Microseconds in a millisecond, as a float.
#define MICROS_PER_MILLIS 1e3

// TODO(chengzhong.wcz): v8 fast api call
AWORKER_METHOD(Hrtime) {
  Isolate* isolate = info.GetIsolate();
  uint64_t time = uv_hrtime();
  info.GetReturnValue().Set(BigInt::NewFromUnsigned(isolate, time));
}

AWORKER_METHOD(GetTimeOrigin) {
  Isolate* isolate = info.GetIsolate();
  info.GetReturnValue().Set(BigInt::NewFromUnsigned(isolate, getTimeOrigin()));
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
  immortal->SetFunctionProperty(exports, "getTimeOrigin", GetTimeOrigin);
  immortal->SetFunctionProperty(
      exports, "getTimeOriginTimeStamp", GetTimeOriginTimeStamp);
  immortal->SetFunctionProperty(exports,
                                "getTimeOriginRelativeTimeStamp",
                                GetTimeOriginRelativeTimeStamp);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  registry->Register(Hrtime);
  registry->Register(GetTimeOrigin);
  registry->Register(GetTimeOriginTimeStamp);
  registry->Register(GetTimeOriginRelativeTimeStamp);
}

}  // namespace aworker

AWORKER_BINDING_REGISTER(perf, aworker::Init, aworker::Init)
