#include "async_wrap.h"
#include "alarm_timer.h"
#include "command_parser_group.h"
#include "error_handling.h"
#include "loop_latency_watchdog.h"

namespace aworker {

using v8::Context;
using v8::EscapableHandleScope;
using v8::Exception;
using v8::Function;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Message;
using v8::Object;
using v8::Promise;
using v8::Undefined;
using v8::Value;

CallbackScope::CallbackScope(Immortal* immortal, Local<Object> resource)
    : immortal_(immortal), resource_(resource) {
  bottom_ = immortal_->callback_scope_stack_top();
  immortal_->set_callback_scope_stack_top(this);
}

CallbackScope::~CallbackScope() {
  immortal_->set_callback_scope_stack_top(bottom_);
}

AsyncWrap::AsyncWrap(Immortal* immortal, Local<Object> handle)
    : BaseObject(immortal, handle) {
  AsyncWrap::EmitAsyncInit(immortal, handle);
}

MaybeLocal<Value> AsyncWrap::MakeCallback(Local<Function> cb,
                                          int argc,
                                          Local<Value>* argv) {
  EscapableHandleScope scope(immortal()->isolate());
  Local<Context> context = immortal()->context();
  Local<Object> recv = object();
  CallbackScope callback_scope(immortal(), recv);

  Local<Function> callback_trampoline =
      immortal()->callback_trampoline_function();

  std::vector<Local<Value>> actual_argv(argc + 1);
  actual_argv[0] = cb;
  for (int idx = 0; idx < argc; idx++) {
    actual_argv[idx + 1] = argv[idx];
  }

  if (immortal()->loop_latency_watchdog()) {
    immortal()->loop_latency_watchdog()->CallbackPrologue();
  }
  MaybeLocal<Value> result =
      callback_trampoline->Call(context, recv, argc + 1, actual_argv.data());
  if (immortal()->loop_latency_watchdog()) {
    immortal()->loop_latency_watchdog()->CallbackEpilogue();
  }
  return scope.EscapeMaybe(result);
}

Local<FunctionTemplate> AsyncWrap::GetConstructorTemplate(Immortal* immortal) {
  Local<FunctionTemplate> tpl = immortal->async_wrap_ctor_template();
  if (tpl.IsEmpty()) {
    tpl = FunctionTemplate::New(immortal->isolate(), nullptr);
    tpl->SetClassName(OneByteString(immortal->isolate(), "AsyncWrap"));
    tpl->Inherit(BaseObject::GetConstructorTemplate(immortal));
    tpl->InstanceTemplate()->SetInternalFieldCount(
        BaseObject::kInternalFieldCount);
    immortal->set_async_wrap_ctor_template(tpl);
  }
  return tpl;
}

void AsyncWrap::AddContext(Local<Context> context) {
  Immortal* immortal = Immortal::GetCurrent(context);
  context->SetPromiseHooks(immortal->promise_init_hook(),
                           immortal->promise_before_hook(),
                           immortal->promise_after_hook(),
                           immortal->promise_resolve_hook());
}

// TODO(chengzhong.wcz): trace support
// TODO(chengzhong.wcz): name/type support
void AsyncWrap::EmitAsyncInit(Immortal* immortal, Local<Object> resource) {
  CHECK(!resource.IsEmpty());

  HandleScope scope(immortal->isolate());
  Local<Function> init_fn = immortal->async_wrap_init_function();

  Local<Value> argv[] = {
      resource,
  };

  TryCatchScope try_catch(immortal, CatchMode::kFatal);
  USE(init_fn->Call(immortal->context(),
                    v8::Undefined(immortal->isolate()),
                    arraysize(argv),
                    argv));
}

AWORKER_METHOD(AsyncWrap::SetJSPromiseHooks) {
  HandleScope scope(info.GetIsolate());
  Immortal* immortal = Immortal::GetCurrent(info);

  immortal->set_promise_init_hook(info[0]->IsFunction() ? info[0].As<Function>()
                                                        : Local<Function>());
  immortal->set_promise_before_hook(
      info[1]->IsFunction() ? info[1].As<Function>() : Local<Function>());
  immortal->set_promise_after_hook(
      info[2]->IsFunction() ? info[2].As<Function>() : Local<Function>());
  immortal->set_promise_resolve_hook(
      info[3]->IsFunction() ? info[3].As<Function>() : Local<Function>());

  AsyncWrap::AddContext(immortal->context());
}

namespace async_wrap {
AWORKER_BINDING(Init) {
  immortal->SetFunctionProperty(
      exports, "setJSPromiseHooks", AsyncWrap::SetJSPromiseHooks);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  registry->Register(AsyncWrap::SetJSPromiseHooks);
}

}  // namespace async_wrap

}  // namespace aworker

AWORKER_BINDING_REGISTER(async_wrap,
                         aworker::async_wrap::Init,
                         aworker::async_wrap::Init)
