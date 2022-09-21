#include "async_wrap.h"
#include "alarm_timer.h"
#include "command_parser_group.h"
#include "error_handling.h"
#include "task_queue.h"

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
  AsyncWrap::EmitAsyncBefore(immortal_, resource_);
}

CallbackScope::~CallbackScope() {
  AsyncWrap::EmitAsyncAfter(immortal_, resource_);
  immortal_->set_callback_scope_stack_top(bottom_);

  task_queue::TickTaskQueue(immortal_);
}

AsyncWrap::AsyncWrap(Immortal* immortal, Local<Object> handle)
    : BaseObject(immortal, handle) {
  CallbackScope* current_scope = immortal->callback_scope_stack_top();
  Local<Value> trigger_resource = Undefined(immortal->isolate());
  if (current_scope != nullptr) {
    trigger_resource = current_scope->resource();
  }
  AsyncWrap::EmitAsyncInit(immortal, handle, trigger_resource);
}

MaybeLocal<Value> AsyncWrap::MakeCallback(Local<Function> cb,
                                          Local<Value> recv,
                                          int argc,
                                          Local<Value>* argv) {
  EscapableHandleScope scope(immortal()->isolate());
  CallbackScope callback_scope(immortal(), object());
  Local<Context> context = immortal()->context();

  MaybeLocal<Value> result = cb->Call(context, recv, argc, argv);
  return scope.EscapeMaybe(result);
}

MaybeLocal<Value> AsyncWrap::MakeCallback(Local<Function> cb,
                                          int argc,
                                          Local<Value>* argv) {
  EscapableHandleScope scope(immortal()->isolate());
  Local<Context> context = immortal()->context();
  Local<Object> recv = object();
  CallbackScope callback_scope(immortal(), recv);

  MaybeLocal<Value> result = cb->Call(context, recv, argc, argv);
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
void AsyncWrap::EmitAsyncInit(Immortal* immortal,
                              Local<Object> resource,
                              Local<Value> trigger_resource) {
  CHECK(!resource.IsEmpty());

  HandleScope scope(immortal->isolate());
  Local<Function> init_fn = immortal->async_wrap_init_function();

  Local<Value> argv[] = {
      resource,
      trigger_resource,
  };

  TryCatchScope try_catch(immortal, CatchMode::kFatal);
  USE(init_fn->Call(
      immortal->context(), immortal->global_object(), arraysize(argv), argv));
}

void AsyncWrap::EmitAsyncBefore(Immortal* immortal, Local<Object> resource) {
  CHECK(!resource.IsEmpty());

  HandleScope scope(immortal->isolate());
  Local<Function> before_fn = immortal->async_wrap_before_function();

  Local<Value> argv[] = {
      resource,
  };

  TryCatchScope try_catch(immortal, CatchMode::kFatal);
  USE(before_fn->Call(
      immortal->context(), immortal->global_object(), arraysize(argv), argv));
}

void AsyncWrap::EmitAsyncAfter(Immortal* immortal, Local<Object> resource) {
  CHECK(!resource.IsEmpty());

  HandleScope scope(immortal->isolate());
  Local<Function> after_fn = immortal->async_wrap_after_function();

  Local<Value> argv[] = {
      resource,
  };

  TryCatchScope try_catch(immortal, CatchMode::kFatal);
  USE(after_fn->Call(
      immortal->context(), immortal->global_object(), arraysize(argv), argv));
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
