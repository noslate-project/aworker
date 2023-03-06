#include <cmath>

#include "aworker_binding.h"
#include "error_handling.h"
#include "handle_wrap.h"
#include "immortal.h"
#include "util.h"
#include "uv.h"

namespace aworker {
namespace Immediate {

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

class ImmediateWrap : public HandleWrap {
  DEFINE_WRAPPERTYPEINFO();
  SIZE_IN_BYTES(ImmediateWrap);
  SET_NO_MEMORY_INFO();

 public:
  static void Init(Local<Object> exports) {
    Immortal* immortal = Immortal::GetCurrent(exports->CreationContext());
    auto isolate = immortal->isolate();
    HandleScope scope(isolate);
    auto context = immortal->context();

    Local<FunctionTemplate> tpl =
        FunctionTemplate::New(isolate, ImmediateWrap::New);
    tpl->Inherit(AsyncWrap::GetConstructorTemplate(immortal));
    tpl->InstanceTemplate()->SetInternalFieldCount(
        BaseObject::kInternalFieldCount);

    Local<String> name = OneByteString(isolate, "ImmediateWrap");

    auto prototype_template = tpl->PrototypeTemplate();
    tpl->SetClassName(name);
    immortal->SetFunctionProperty(
        prototype_template, "remove", ImmediateWrap::Remove);

    exports->Set(context, name, tpl->GetFunction(context).ToLocalChecked())
        .Check();
  }

  static void Init(ExternalReferenceRegistry* registry) {
    registry->Register(ImmediateWrap::New);
    registry->Register(ImmediateWrap::Remove);
  }

  static AWORKER_METHOD(New) {
    Immortal* immortal = Immortal::GetCurrent(info);
    auto isolate = immortal->isolate();
    auto context = immortal->context();
    HandleScope scope(isolate);

    CHECK(info.Length() == 0);

    new ImmediateWrap(immortal, info.This());

    info.GetReturnValue().Set(info.This());
  }

  static AWORKER_METHOD(Remove) {
    Immortal* immortal = Immortal::GetCurrent(info);
    auto isolate = immortal->isolate();
    HandleScope scope(isolate);
    ImmediateWrap* wrap;
    ASSIGN_OR_RETURN_UNWRAP(&wrap, info.This());
    wrap->MakeWeak();

    uv_idle_stop(&wrap->idle);
    uv_check_stop(&wrap->check);
  }

  ImmediateWrap(Immortal* immortal, Local<Object> object)
      : HandleWrap(immortal, object, reinterpret_cast<uv_handle_t*>(&check)) {
    uv_check_init(immortal->event_loop(), &check);
    uv_idle_init(immortal->event_loop(), &idle);
    idle.data = reinterpret_cast<void*>(0x10012);
    uv_idle_start(&idle, [](uv_idle_t* idle) { /* idling */ });
    uv_check_start(&check, TriggerImmediate);
  }

  ~ImmediateWrap() = default;

 private:
  static void TriggerImmediate(uv_check_t* check) {
    ImmediateWrap* handle = ContainerOf(&ImmediateWrap::check, check);
    auto isolate = handle->immortal()->isolate();
    HandleScope scope(isolate);
    handle->MakeCallback(handle->immortal()->onimmediate_string(), 0, nullptr);

    uv_idle_stop(&handle->idle);
    uv_check_stop(&handle->check);

    // uv_close(reinterpret_cast<uv_handle_t*>(&handle->check),
    //          [](uv_handle_t* handle0) {});
    // uv_close(reinterpret_cast<uv_handle_t*>(&handle->idle),
    //          [](uv_handle_t* handle1) {});
  }

  uv_check_t check;
  uv_idle_t idle;
};

const WrapperTypeInfo ImmediateWrap::wrapper_type_info_{
    "immediate_wrap",
};

AWORKER_BINDING(Init) {
  ImmediateWrap::Init(exports);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  ImmediateWrap::Init(registry);
}

}  // namespace Immediate
}  // namespace aworker

AWORKER_BINDING_REGISTER(immediate,
                         aworker::Immediate::Init,
                         aworker::Immediate::Init);