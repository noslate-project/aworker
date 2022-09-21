#include <cmath>

#include "aworker_binding.h"
#include "error_handling.h"
#include "handle_wrap.h"
#include "immortal.h"
#include "util.h"
#include "uv.h"

namespace aworker {
namespace Timers {

using v8::BigInt;
using v8::Boolean;
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Global;
using v8::HandleScope;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Number;
using v8::Object;
using v8::String;
using v8::TryCatch;
using v8::Value;

class TimerWrap : public HandleWrap {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static void Init(Local<Object> exports) {
    Immortal* immortal = Immortal::GetCurrent(exports->CreationContext());
    auto isolate = immortal->isolate();
    HandleScope scope(isolate);
    auto context = immortal->context();

    Local<FunctionTemplate> tpl =
        FunctionTemplate::New(isolate, TimerWrap::New);
    tpl->Inherit(AsyncWrap::GetConstructorTemplate(immortal));
    tpl->InstanceTemplate()->SetInternalFieldCount(
        BaseObject::kInternalFieldCount);

    Local<String> name = OneByteString(isolate, "TimerWrap");

    auto prototype_template = tpl->PrototypeTemplate();
    tpl->SetClassName(name);
    immortal->SetFunctionProperty(
        prototype_template, "remove", TimerWrap::Remove);

    exports->Set(context, name, tpl->GetFunction(context).ToLocalChecked())
        .Check();
  }

  static void Init(ExternalReferenceRegistry* registry) {
    registry->Register(TimerWrap::New);
    registry->Register(TimerWrap::Remove);
  }

  static AWORKER_METHOD(New) {
    Immortal* immortal = Immortal::GetCurrent(info);
    auto isolate = immortal->isolate();
    auto context = immortal->context();
    HandleScope scope(isolate);

    CHECK(info.Length() >= 2 && info[0]->IsNumber() && info[1]->IsBoolean());

    Local<Number> interval = info[0].As<Number>();
    Local<Boolean> is_loop = info[1].As<Boolean>();

    Maybe<int> maybe_interval = interval->Int32Value(context);
    CHECK(maybe_interval.IsJust());
    int int_interval = maybe_interval.FromJust();
    bool bool_is_loop = is_loop->BooleanValue(isolate);

    new TimerWrap(immortal, info.This(), bool_is_loop, int_interval);

    info.GetReturnValue().Set(info.This());
  }

  static AWORKER_METHOD(Remove) {
    Immortal* immortal = Immortal::GetCurrent(info);
    auto isolate = immortal->isolate();
    HandleScope scope(isolate);
    TimerWrap* wrap;
    ASSIGN_OR_RETURN_UNWRAP(&wrap, info.This());
    wrap->MakeWeak();

    uv_timer_stop(&wrap->timer);
  }

  TimerWrap(Immortal* immortal,
            Local<Object> object,
            bool repeat,
            uint64_t interval)
      : HandleWrap(immortal, object, reinterpret_cast<uv_handle_t*>(&timer)) {
    uv_timer_init(immortal->event_loop(), &timer);
    uv_timer_start(&timer, TriggerTimer, interval, repeat ? interval : 0);
  }

  ~TimerWrap() = default;

 private:
  static void TriggerTimer(uv_timer_t* timer) {
    TimerWrap* handle = ContainerOf(&TimerWrap::timer, timer);
    auto isolate = handle->immortal()->isolate();
    HandleScope scope(isolate);

    // Timer callback may throw. However those exceptions were routed to global
    // scope, we don't care about the return value here;
    handle->MakeCallback(handle->immortal()->ontimeout_string(), 0, nullptr);
  }

  uv_timer_t timer;
};

const WrapperTypeInfo TimerWrap::wrapper_type_info_{
    "timer_wrap",
};

AWORKER_BINDING(Init) {
  TimerWrap::Init(exports);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  TimerWrap::Init(registry);
}

}  // namespace Timers
}  // namespace aworker

AWORKER_BINDING_REGISTER(timers, aworker::Timers::Init, aworker::Timers::Init)
