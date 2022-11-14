#include <cmath>

#include "aworker_binding.h"
#include "error_handling.h"
#include "handle_wrap.h"
#include "immortal.h"
#include "util.h"
#include "uv.h"

namespace aworker {
namespace Signal {

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

class SignalWrap : public HandleWrap {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static void Init(Local<Object> exports) {
    Immortal* immortal = Immortal::GetCurrent(exports->CreationContext());
    auto isolate = immortal->isolate();
    HandleScope scope(isolate);
    auto context = immortal->context();

    Local<FunctionTemplate> tpl =
        FunctionTemplate::New(isolate, SignalWrap::New);
    tpl->Inherit(AsyncWrap::GetConstructorTemplate(immortal));
    tpl->InstanceTemplate()->SetInternalFieldCount(
        BaseObject::kInternalFieldCount);

    Local<String> name = OneByteString(isolate, "SignalWrap");

    auto prototype_template = tpl->PrototypeTemplate();
    tpl->SetClassName(name);
    exports->Set(context, name, tpl->GetFunction(context).ToLocalChecked())
        .Check();
  }
  static void Init(ExternalReferenceRegistry* registry) {
    registry->Register(SignalWrap::New);
  }

  static AWORKER_METHOD(New) {
    Immortal* immortal = Immortal::GetCurrent(info);

    CHECK(info.Length() == 2 && info[0]->IsInt32() && info[1]->IsFunction());

    Local<v8::Int32> signal_num = info[0].As<v8::Int32>();
    Local<Function> cb = info[1].As<Function>();

    new SignalWrap(immortal, info.This(), signal_num->Value());

    info.GetReturnValue().Set(info.This());
  }

  SignalWrap(Immortal* immortal, Local<Object> object, int32_t sig_num)
      : HandleWrap(immortal, object, reinterpret_cast<uv_handle_t*>(&signal_)) {
    uv_signal_init(immortal->event_loop(), &signal_);
    uv_signal_start(&signal_, SignalCallback, sig_num);
    uv_unref(reinterpret_cast<uv_handle_t*>(&signal_));
  }

  ~SignalWrap() = default;

 private:
  static void SignalCallback(uv_signal_t* signal, int signum) {
    SignalWrap* handle = ContainerOf(&SignalWrap::signal_, signal);
    auto isolate = handle->immortal()->isolate();
    HandleScope scope(isolate);

    // Timer callback may throw. However those exceptions were routed to global
    // scope, we don't care about the return value here;
    handle->MakeCallback(handle->immortal()->on_signal_string(), 0, nullptr);
  }

  uv_signal_t signal_;
};

// uv_signal_t SignalWrap::signal_;

const WrapperTypeInfo SignalWrap::wrapper_type_info_{
    "signal_wrap",
};

AWORKER_BINDING(Init) {
  SignalWrap::Init(exports);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  SignalWrap::Init(registry);
}

}  // namespace Signal
}  // namespace aworker

AWORKER_BINDING_REGISTER(signals, aworker::Signal::Init, aworker::Signal::Init)
