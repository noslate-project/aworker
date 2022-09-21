#include "handle_wrap.h"
#include "async_wrap.h"
#include "immortal.h"
#include "util.h"

namespace aworker {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Local;
using v8::Object;
using v8::ObjectTemplate;
using v8::Value;

void HandleWrap::Ref(const FunctionCallbackInfo<Value>& args) {
  HandleWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  if (IsAlive(wrap)) uv_ref(wrap->handle());
}

void HandleWrap::Unref(const FunctionCallbackInfo<Value>& args) {
  HandleWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  if (IsAlive(wrap)) uv_unref(wrap->handle());
}

void HandleWrap::HasRef(const FunctionCallbackInfo<Value>& args) {
  HandleWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  args.GetReturnValue().Set(HasRef(wrap));
}

void HandleWrap::Close(const FunctionCallbackInfo<Value>& args) {
  HandleWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  wrap->Close(args[0]);
}

void HandleWrap::Close(Local<Value> close_callback) {
  if (_state != kInitialized) return;

  uv_close(_handle, OnClose);
  _state = kClosing;

  if (!close_callback.IsEmpty() && close_callback->IsFunction() &&
      !persistent().IsEmpty()) {
    object()
        ->Set(immortal()->context(),
              immortal()->handle_onclose_string(),
              close_callback)
        .Check();
  }
}

void HandleWrap::OnGCCollect() {
  Close();
}

HandleWrap::HandleWrap(Immortal* immortal,
                       Local<Object> object,
                       uv_handle_t* handle)
    : AsyncWrap(immortal, object), _state(kInitialized), _handle(handle) {
  _handle->data = this;
  immortal->tracked_handle_wraps.insert(this);
}

void HandleWrap::OnClose(uv_handle_t* handle) {
  CHECK_NOT_NULL(handle->data);
  HandleWrap* wrap = static_cast<HandleWrap*>(handle->data);

  Immortal* immortal = wrap->immortal();
  HandleScope scope(immortal->isolate());
  Local<Context> context = immortal->context();
  Context::Scope context_scope(context);

  CHECK_EQ(wrap->_state, kClosing);

  wrap->_state = kClosed;
  immortal->tracked_handle_wraps.erase(wrap);

  wrap->OnClose();

  if (!wrap->persistent().IsEmpty() &&
      wrap->object()
          ->Has(context, immortal->handle_onclose_string())
          .FromMaybe(false)) {
    // TODO(chengzhong.wcz): use symbol?
    wrap->MakeCallback(immortal->handle_onclose_string(), 0, nullptr);
  }

  delete wrap;
}

Local<FunctionTemplate> HandleWrap::GetConstructorTemplate(Immortal* immortal) {
  Local<FunctionTemplate> tpl = immortal->handle_wrap_ctor_template();
  if (tpl.IsEmpty()) {
    tpl = FunctionTemplate::New(immortal->isolate(), nullptr);
    tpl->SetClassName(OneByteString(immortal->isolate(), "HandleWrap"));
    tpl->Inherit(AsyncWrap::GetConstructorTemplate(immortal));
    tpl->InstanceTemplate()->SetInternalFieldCount(
        BaseObject::kInternalFieldCount);

    Local<ObjectTemplate> proto = tpl->PrototypeTemplate();
    immortal->SetFunctionProperty(proto, "close", HandleWrap::Close);
    immortal->SetFunctionProperty(proto, "hasRef", HandleWrap::HasRef);
    immortal->SetFunctionProperty(proto, "ref", HandleWrap::Ref);
    immortal->SetFunctionProperty(proto, "unref", HandleWrap::Unref);
    immortal->set_handle_wrap_ctor_template(tpl);
  }
  return tpl;
}

}  // namespace aworker
