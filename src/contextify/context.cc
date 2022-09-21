#include "context.h"
#include "alarm_timer.h"
#include "async_wrap.h"
#include "debug_utils.h"
#include "error_handling.h"
#include "inspector/inspector_agent.h"
#include "script.h"

namespace aworker {
namespace contextify {

using std::string;
using v8::Array;
using v8::Boolean;
using v8::EscapableHandleScope;
using v8::FunctionTemplate;
using v8::Global;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Name;
using v8::NamedPropertyHandlerConfiguration;
using v8::Object;
using v8::ObjectTemplate;
using v8::PropertyAttribute;
using v8::PropertyCallbackInfo;
using v8::PropertyDescriptor;
using v8::String;
using v8::UnboundScript;
using v8::Value;
using v8::WeakCallbackInfo;

const WrapperTypeInfo ContextWrap::wrapper_type_info_{
    "context_wrap",
};

ContextWrap::ContextWrap(Immortal* immortal,
                         Local<Object> object,
                         Local<Object> sandbox,
                         const string& name,
                         const string& origin)
    : BaseObject(immortal, object) {
  Local<v8::Context> v8_context = CreateV8Context(sandbox, name, origin);
  context_.Reset(immortal->isolate(), v8_context);
  MakeWeak();
}

ContextWrap::ContextWrap(Immortal* immortal,
                         Local<Object> object,
                         Local<v8::Context> v8_context)
    : BaseObject(immortal, object) {
  context_.Reset(immortal->isolate(), v8_context);
  MakeWeak();
}

ContextWrap::~ContextWrap() {}

void ContextWrap::MakeWeak() {
  BaseObject::MakeWeak();
  context_.SetWeak();
}

Local<v8::Context> ContextWrap::CreateV8Context(Local<Object> sandbox,
                                                const string& name,
                                                const string& _origin) {
  Isolate* isolate = immortal()->isolate();
  EscapableHandleScope scope(isolate);
  Local<FunctionTemplate> function_template = FunctionTemplate::New(isolate);
  function_template->SetClassName(sandbox->GetConstructorName());

  Local<ObjectTemplate> object_template = function_template->InstanceTemplate();

  NamedPropertyHandlerConfiguration config(
      GlobalPropertyGetterCallback,
      GlobalPropertySetterCallback,
      GlobalPropertyDescriptorCallback,
      GlobalPropertyDeleterCallback,
      GlobalPropertyEnumeratorCallback,
      GlobalPropertyDefinerCallback,
      {},
      v8::PropertyHandlerFlags::kHasNoSideEffect);
  object_template->SetHandler(config);

  Local<v8::Context> ctx = v8::Context::New(isolate, nullptr, object_template);

  ctx->SetSecurityToken(immortal()->context()->GetSecurityToken());

  immortal()->AssignToContext(ctx);
  ctx->SetAlignedPointerInEmbedderData(ContextEmbedderIndex::kContextifyContext,
                                       this);
  // tie ctx with sandbox and bound context object's lifetime
  ctx->SetEmbedderData(ContextEmbedderIndex::kSandboxObject, sandbox);
  ctx->SetEmbedderData(ContextEmbedderIndex::kJavaScriptContextObject,
                       object());

  // sign `sandbox` as `contextified`
  sandbox->SetPrivate(immortal()->context(),
                      immortal()->contextify_global_private_symbol(),
                      ctx->Global());

  AsyncWrap::AddContext(ctx);

  return scope.Escape(ctx);
}

void ContextWrap::GlobalPropertyGetterCallback(
    Local<Name> property, const PropertyCallbackInfo<Value>& args) {
  ContextWrap* ctx = ContextWrap::Get(args);

  Local<v8::Context> context = ctx->context();
  Local<Object> sandbox = ctx->sandbox();

  MaybeLocal<Value> maybe_rv = sandbox->GetRealNamedProperty(context, property);
  if (maybe_rv.IsEmpty()) {
    maybe_rv = ctx->global()->GetRealNamedProperty(context, property);
  }

  Local<Value> rv;
  if (maybe_rv.ToLocal(&rv)) {
    if (rv == sandbox) {
      rv = ctx->global();
    }

    args.GetReturnValue().Set(rv);
  }
}

void ContextWrap::GlobalPropertySetterCallback(
    Local<Name> property,
    Local<Value> value,
    const PropertyCallbackInfo<Value>& args) {
  ContextWrap* ctx = ContextWrap::Get(args);

  Local<v8::Context> context = ctx->context();

  PropertyAttribute attributes = PropertyAttribute::None;
  bool is_declared_on_global_proxy =
      ctx->global()
          ->GetRealNamedPropertyAttributes(context, property)
          .To(&attributes);
  bool read_only = static_cast<int>(attributes) &
                   static_cast<int>(PropertyAttribute::ReadOnly);

  bool is_declared_on_sandbox =
      ctx->sandbox()
          ->GetRealNamedPropertyAttributes(context, property)
          .To(&attributes);
  read_only = read_only || (static_cast<int>(attributes) &
                            static_cast<int>(PropertyAttribute::ReadOnly));

  if (read_only) return;

  // true for x = 5
  // false for this.x = 5
  // false for Object.defineProperty(this, 'foo', ...)
  // false for vmResult.x = 5 where vmResult = vm.runInContext();
  bool is_contextual_store = ctx->global() != args.This();

  // Indicator to not return before setting (undeclared) function declarations
  // on the sandbox in strict mode, i.e. args.ShouldThrowOnError() = true.
  // True for 'function f() {}', 'this.f = function() {}',
  // 'var f = function()'.
  // In effect only for 'function f() {}' because
  // var f = function(), is_declared = true
  // this.f = function() {}, is_contextual_store = false.
  bool is_function = value->IsFunction();

  bool is_declared = is_declared_on_global_proxy || is_declared_on_sandbox;
  if (!is_declared && args.ShouldThrowOnError() && is_contextual_store &&
      !is_function) {
    return;
  }

  if (!is_declared_on_global_proxy && is_declared_on_sandbox &&
      args.ShouldThrowOnError() && is_contextual_store && !is_function) {
    // The property exists on the sandbox but not on the global
    // proxy. Setting it would throw because we are in strict mode.
    // Don't attempt to set it by signaling that the call was
    // intercepted. Only change the value on the sandbox.
    args.GetReturnValue().Set(false);
  }

  USE(ctx->sandbox()->Set(ctx->context(), property, value));
}

void ContextWrap::GlobalPropertyDescriptorCallback(
    Local<Name> property, const PropertyCallbackInfo<Value>& args) {
  ContextWrap* ctx = ContextWrap::Get(args);

  Local<v8::Context> context = ctx->context();
  Local<Object> sandbox = ctx->sandbox();

  if (sandbox->HasOwnProperty(context, property).FromMaybe(false)) {
    Local<Value> desc;
    if (sandbox->GetOwnPropertyDescriptor(context, property).ToLocal(&desc)) {
      args.GetReturnValue().Set(desc);
    }
  }
}

void ContextWrap::GlobalPropertyDefinerCallback(
    Local<Name> property,
    const PropertyDescriptor& desc,
    const PropertyCallbackInfo<Value>& args) {
  ContextWrap* ctx = ContextWrap::Get(args);

  Local<v8::Context> context = ctx->context();
  Isolate* isolate = context->GetIsolate();

  PropertyAttribute attributes = PropertyAttribute::None;
  bool is_declared =
      ctx->global()
          ->GetRealNamedPropertyAttributes(ctx->context(), property)
          .To(&attributes);
  bool read_only = static_cast<int>(attributes) &
                   static_cast<int>(PropertyAttribute::ReadOnly);

  // If the property is set on the global as read_only, don't change it on
  // the global or sandbox.
  if (is_declared && read_only) return;

  Local<Object> sandbox = ctx->sandbox();

  auto define_prop_on_sandbox = [&](PropertyDescriptor* desc_for_sandbox) {
    if (desc.has_enumerable()) {
      desc_for_sandbox->set_enumerable(desc.enumerable());
    }
    if (desc.has_configurable()) {
      desc_for_sandbox->set_configurable(desc.configurable());
    }
    // Set the property on the sandbox.
    USE(sandbox->DefineProperty(context, property, *desc_for_sandbox));
  };

  if (desc.has_get() || desc.has_set()) {
    PropertyDescriptor desc_for_sandbox(
        desc.has_get() ? desc.get() : Undefined(isolate).As<Value>(),
        desc.has_set() ? desc.set() : Undefined(isolate).As<Value>());

    define_prop_on_sandbox(&desc_for_sandbox);
  } else {
    Local<Value> value =
        desc.has_value() ? desc.value() : Undefined(isolate).As<Value>();

    if (desc.has_writable()) {
      PropertyDescriptor desc_for_sandbox(value, desc.writable());
      define_prop_on_sandbox(&desc_for_sandbox);
    } else {
      PropertyDescriptor desc_for_sandbox(value);
      define_prop_on_sandbox(&desc_for_sandbox);
    }
  }
}

void ContextWrap::GlobalPropertyQueryCallback(
    Local<Name> property, const PropertyCallbackInfo<Integer>& args) {
  ContextWrap* ctx = ContextWrap::Get(args);

  Local<v8::Context> context = ctx->context();
  Maybe<PropertyAttribute> maybe_prop_attr =
      ctx->sandbox()->GetRealNamedPropertyAttributes(context, property);

  if (maybe_prop_attr.IsNothing()) {
    maybe_prop_attr =
        ctx->global()->GetRealNamedPropertyAttributes(context, property);
  }

  if (maybe_prop_attr.IsJust()) {
    PropertyAttribute prop_attr = maybe_prop_attr.FromJust();
    args.GetReturnValue().Set(prop_attr);
  }
}

void ContextWrap::GlobalPropertyDeleterCallback(
    Local<Name> property, const PropertyCallbackInfo<Boolean>& args) {
  ContextWrap* ctx = ContextWrap::Get(args);

  Maybe<bool> success = ctx->sandbox()->Delete(ctx->context(), property);

  if (success.FromMaybe(false)) return;

  // Delete failed on the sandbox, intercept and do not delete on
  // the global object.
  args.GetReturnValue().Set(false);
}

void ContextWrap::GlobalPropertyEnumeratorCallback(
    const PropertyCallbackInfo<Array>& args) {
  ContextWrap* ctx = ContextWrap::Get(args);
  Local<Array> properties;
  if (!ctx->sandbox()->GetPropertyNames(ctx->context()).ToLocal(&properties))
    return;

  args.GetReturnValue().Set(properties);
}

AWORKER_METHOD(ContextWrap::New) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  if (info.Length() == 0) {
    new ContextWrap(immortal, info.This(), immortal->context());
    info.GetReturnValue().Set(info.This());
    return;
  }

  Local<Object> sandbox = info[0].As<Object>();

  Local<String> v8_name = info[1].As<String>();
  Local<String> v8_origin = info[2].As<String>();

  String::Utf8Value name(isolate, v8_name);
  String::Utf8Value origin(isolate, v8_origin);

  new ContextWrap(immortal, info.This(), sandbox, *name, *origin);

  info.GetReturnValue().Set(info.This());
}

AWORKER_METHOD(ContextWrap::GetGlobalThis) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  ContextWrap* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, info.This());
  info.GetReturnValue().Set(ctx->context()->Global());
}

AWORKER_METHOD(ContextWrap::Execute) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  Local<v8::Context> context = immortal->context();
  HandleScope scope(isolate);

  ContextWrap* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, info.This());

  int64_t timeout = info[1]->IntegerValue(context).FromJust();
  ExecutionFlags flag =
      static_cast<ExecutionFlags>(info[2]->IntegerValue(context).FromJust());

  if (flag & ExecutionFlags::kBreakOnFirstLine) {
    immortal->inspector_agent()->PauseOnNextJavascriptStatement(
        "Break at entry");
  }

  v8::Context::Scope context_scope(ctx->context());
  MaybeLocal<Value> result;
  if (flag & ExecutionFlags::kModule) {
    ModuleWrap* mod;
    ASSIGN_OR_RETURN_UNWRAP(&mod, info[0].As<Object>());
    result = DoExecute(immortal, mod, timeout, flag);
  } else {
    ScriptWrap* script;
    ASSIGN_OR_RETURN_UNWRAP(&script, info[0].As<Object>());
    result = DoExecute(immortal, script, timeout, flag);
  }
  if (!result.IsEmpty()) {
    info.GetReturnValue().Set(result.ToLocalChecked());
  }
}

struct ExecuteTimeoutContext {
  v8::Isolate* isolate;
  volatile sig_atomic_t timeout;
};

void DoExecuteTimeoutHandler(AlarmTimer* timer, void* _context) {
  ExecuteTimeoutContext* context =
      static_cast<ExecuteTimeoutContext*>(_context);
  context->timeout = 1;
  context->isolate->TerminateExecution();
}

MaybeLocal<Value> ContextWrap::DoExecute(Immortal* immortal,
                                         ScriptWrap* script,
                                         const int64_t timeout,
                                         const ExecutionFlags flag) {
  Isolate* isolate = immortal->isolate();

  Local<UnboundScript> unbound_script = script->script();
  Local<v8::Script> bound_script = unbound_script->BindToCurrentContext();

  ExecuteTimeoutContext timeout_context = {isolate, 0};
  uint64_t timer_id = 0;

  if (timeout != 0) {
    int ret = AlarmTimerLathe::Instance().create_timer(
        timeout, DoExecuteTimeoutHandler, &timeout_context, &timer_id);
    if (ret != 0) {
      char err[1024];
      snprintf(err, sizeof(err), "%s", strerror(ret));
      ThrowException(isolate, err);
      return MaybeLocal<Value>();
    }
  }

  MaybeLocal<Value> result = bound_script->Run(immortal->context());
  if (timeout != 0) {
    if (timeout_context.timeout) {
      ThrowException(immortal->isolate(), "Script execution timed out");
    } else {
      AlarmTimerLathe::Instance().close(timer_id);
    }
  }

  return result;
}

MaybeLocal<Value> ContextWrap::DoExecute(Immortal* immortal,
                                         ModuleWrap* module,
                                         const int64_t timeout,
                                         const ExecutionFlags flag) {
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  Local<v8::Context> context = immortal->context();

  MaybeLocal<Value> result = module->module()->Evaluate(context);

  // v8::Module::Evaluate returned a non-javascript-value, which fails
  // return value verify DCHECK.
  return v8::Boolean::New(isolate, result.IsEmpty());
}

AWORKER_BINDING(ContextInit) {
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  Local<FunctionTemplate> tpl =
      FunctionTemplate::New(isolate, ContextWrap::New);
  tpl->InstanceTemplate()->SetInternalFieldCount(
      BaseObject::kInternalFieldCount);

  MaybeLocal<String> maybe_name = String::NewFromUtf8(isolate, "ContextWrap");
  CHECK(!maybe_name.IsEmpty());
  Local<String> name = maybe_name.ToLocalChecked();
  tpl->SetClassName(name);

  Local<ObjectTemplate> prototype_template = tpl->PrototypeTemplate();
  immortal->SetFunctionProperty(
      prototype_template, "globalThis", ContextWrap::GetGlobalThis);
  immortal->SetFunctionProperty(
      prototype_template, "execute", ContextWrap::Execute);

  exports->Set(context, name, tpl->GetFunction(context).ToLocalChecked())
      .Check();

  Local<Object> flags = Object::New(isolate);
#define V(name)                                                                \
  immortal->SetIntegerProperty(flags, #name, ExecutionFlags::name);
  V(kNone);
  V(kBreakOnFirstLine);
  V(kModule);
#undef V
  immortal->SetValueProperty(exports, "ExecutionFlags", flags);
}

AWORKER_EXTERNAL_REFERENCE(ContextInit) {
  registry->Register(ContextWrap::New);
  registry->Register(ContextWrap::GetGlobalThis);
  registry->Register(ContextWrap::Execute);
}

}  // namespace contextify
}  // namespace aworker
