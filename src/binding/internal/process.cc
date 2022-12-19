#include "binding/internal/process.h"

#include "aworker.h"
#include "aworker_binding.h"
#include "aworker_version.h"
#include "command_parser.h"
#include "error_handling.h"
#include "immortal.h"
#include "uv.h"

using v8::Array;
using v8::BigInt;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Int32;
using v8::Isolate;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Name;
using v8::Number;
using v8::Object;
using v8::PropertyCallbackInfo;
using v8::String;
using v8::Value;

namespace aworker {
namespace process {

AWORKER_METHOD(Exit) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();

  int32_t code = 0;
  if (info.Length()) {
    if (!info[0]->IsInt32()) {
      ThrowException(isolate,
                     "Exit code should be an integer.",
                     ExceptionType::kTypeError);
      return;
    }

    code = info[0].As<Int32>()->Value();
  }

  // 马上退出
  exit(code);
}

AWORKER_METHOD(Cwd) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  Local<String> cwd =
      String::NewFromUtf8(isolate, aworker::cwd().data()).ToLocalChecked();
  info.GetReturnValue().Set(cwd);
}

AWORKER_METHOD(ScriptFilename) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  Local<String> filename =
      String::NewFromUtf8(
          isolate, immortal->commandline_parser()->script_filename().c_str())
          .ToLocalChecked();
  info.GetReturnValue().Set(filename);
}

AWORKER_METHOD(ConsoleCall) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal->context();

  SlicedArguments call_args(info, 2);
  if (immortal->inspector_agent() != nullptr) {
    Local<Value> inspector_method = info[0];
    CHECK(inspector_method->IsFunction());
    MaybeLocal<Value> ret = inspector_method.As<Function>()->Call(
        context, info.Holder(), call_args.size(), call_args.data());
    if (ret.IsEmpty()) return;
  }

  Local<Value> internal_method = info[1];
  CHECK(internal_method->IsFunction());

  // TODO(chengzhong.wcz): safe Local;
  internal_method.As<Function>()
      ->Call(context, info.Holder(), call_args.size(), call_args.data())
      .FromMaybe(Local<Value>());
}

AWORKER_GETTER(HasAgent) {
  Immortal* immortal = Immortal::GetCurrent(info);
  bool has = immortal->agent_data_channel() != nullptr;
  info.GetReturnValue().Set(has);
}

AWORKER_GETTER(GetExternalSnapshotLoaded) {
  info.GetReturnValue().Set(per_process::external_snapshot_loaded);
}

AWORKER_GETTER(GetStartupForkMode) {
  Immortal* immortal = Immortal::GetCurrent(info);
  info.GetReturnValue().Set(
      static_cast<uint32_t>(immortal->startup_fork_mode()));
}

enum JAVASCRIPT_STD_NUM { STDOUT = 0, STDERR = 1 };

inline FILE* get_std(JAVASCRIPT_STD_NUM _std) {
  switch (_std) {
    case JAVASCRIPT_STD_NUM::STDERR:
      return stderr;

    case JAVASCRIPT_STD_NUM::STDOUT:
    default:
      return stdout;
  }
}

AWORKER_METHOD(WriteStdxxx) {
  Immortal* immortal = Immortal::GetCurrent(info);
  auto isolate = immortal->isolate();
  auto context = immortal->context();
  HandleScope scope(isolate);

  if (info.Length() < 2 || !info[0]->IsNumber() || !info[1]->IsString()) {
    ThrowException(
        isolate, "writeStdxxx arguments error.", ExceptionType::kTypeError);
    return;
  }

  auto arg0 = info[0].As<Number>()->Int32Value(context).FromJust();

  FILE* stdxxx = get_std(static_cast<JAVASCRIPT_STD_NUM>(arg0));
  String::Utf8Value content_value(isolate, info[1].As<String>());

  fputs(*content_value, stdxxx);
}

AWORKER_METHOD(IsATTY) {
  Immortal* immortal = Immortal::GetCurrent(info);
  auto isolate = immortal->isolate();
  auto context = immortal->context();
  HandleScope scope(isolate);

  if (!info.Length() || !info[0]->IsInt32()) {
    ThrowException(
        isolate, "isatty argument error.", ExceptionType::kTypeError);
    return;
  }

  Maybe<int32_t> maybe_std = info[0].As<Int32>()->Int32Value(context);
  CHECK(maybe_std.IsJust());
  int32_t _std = maybe_std.FromJust();
  FILE* std_file = get_std(static_cast<JAVASCRIPT_STD_NUM>(_std));

  bool ret = isatty(fileno(std_file));
  info.GetReturnValue().Set(ret);
}

AWORKER_METHOD(SetWorkerState) {
  Immortal* immortal = Immortal::GetCurrent(info);
  auto isolate = immortal->isolate();
  auto context = immortal->context();
  HandleScope scope(isolate);

  uint32_t val = info[0].As<v8::Uint32>()->Uint32Value(context).ToChecked();
  CHECK(val <= static_cast<uint32_t>(WorkerState::kActivated));
  // The state value is monotonic over time.
  CHECK(static_cast<uint32_t>(immortal->worker_state()) < val);
  WorkerState state = static_cast<WorkerState>(val);
  immortal->set_worker_state(state);

  // Only stop the loop when the loop is still alive to avoid propagating the
  // stopping flag to the next run.
  if (state == WorkerState::kForkPrepared &&
      uv_loop_alive(immortal->event_loop())) {
    uv_stop(immortal->event_loop());
  }
}

#define METADATA_KEYS(V)                                                       \
  V(AWORKER_PLATFORM, platform)                                                \
  V(AWORKER_ARCH, arch)                                                        \
  V(AWORKER_VERSION, version)
#define V(VAL, KEY)                                                            \
  AWORKER_GETTER(Get##KEY) {                                                   \
    Immortal* immortal = Immortal::GetCurrent(info);                           \
    Isolate* isolate = immortal->isolate();                                    \
    HandleScope scope(isolate);                                                \
    Local<String> val = String::NewFromUtf8(isolate, VAL).ToLocalChecked();    \
    info.GetReturnValue().Set(val);                                            \
  }
METADATA_KEYS(V)
#undef V

#define ImmortalFunctions(V)                                                   \
  V(async_wrap_init_function, asyncWrapInitFunction)                           \
  V(async_wrap_before_function, asyncWrapBeforeFunction)                       \
  V(async_wrap_after_function, asyncWrapAfterFunction)                         \
  V(dom_exception_constructor, domExceptionConstructor)

#define V(NAME, _)                                                             \
  AWORKER_SETTER(SetImmortalFunction##NAME) {                                  \
    Immortal* immortal = Immortal::GetCurrent(info);                           \
    Local<Function> fn = value.As<Function>();                                 \
    immortal->set_##NAME(fn);                                                  \
  }                                                                            \
  AWORKER_GETTER(GetImmortalFunction##NAME) {                                  \
    Immortal* immortal = Immortal::GetCurrent(info);                           \
    info.GetReturnValue().Set(immortal->NAME());                               \
  }
ImmortalFunctions(V);
#undef V

AWORKER_BINDING(Init) {
  immortal->set_process_object(exports);

  immortal->SetFunctionProperty(exports, "exit", Exit);
  immortal->SetFunctionProperty(exports, "cwd", Cwd);
  immortal->SetFunctionProperty(exports, "scriptFilename", ScriptFilename);
  immortal->SetFunctionProperty(exports, "consoleCall", ConsoleCall);

  immortal->SetAccessor(exports, "_hasAgent", HasAgent);
  immortal->SetAccessor(
      exports, "externalSnapshotLoaded", GetExternalSnapshotLoaded);
  immortal->SetAccessor(exports, "startupForkMode", GetStartupForkMode);

  immortal->SetFunctionProperty(exports, "writeStdxxx", WriteStdxxx);
  immortal->SetFunctionProperty(exports, "isATTY", IsATTY);

  immortal->SetFunctionProperty(exports, "setWorkerState", SetWorkerState);

#define V(VAL, KEY) immortal->SetAccessor(exports, #KEY, Get##KEY);
  METADATA_KEYS(V)
#undef V

#define V(IT, NAME)                                                            \
  immortal->SetAccessor(                                                       \
      exports, #NAME, GetImmortalFunction##IT, SetImmortalFunction##IT);
  ImmortalFunctions(V);
#undef V

  Local<String> env_string = aworker::OneByteString(immortal->isolate(), "env");
  Local<Object> env_var_proxy =
      CreateEnvVarProxy(immortal->context(), immortal->isolate())
          .ToLocalChecked();
  exports->Set(immortal->context(), env_string, env_var_proxy).ToChecked();
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  registry->Register(Exit);
  registry->Register(Cwd);
  registry->Register(ScriptFilename);
  registry->Register(ConsoleCall);
  registry->Register(HasAgent);
  registry->Register(GetExternalSnapshotLoaded);
  registry->Register(GetStartupForkMode);
  registry->Register(WriteStdxxx);
  registry->Register(IsATTY);
  registry->Register(SetWorkerState);

#define V(VAL, KEY) registry->Register(Get##KEY);
  METADATA_KEYS(V)
#undef V

#define V(IT, NAME)                                                            \
  registry->Register(GetImmortalFunction##IT);                                 \
  registry->Register(SetImmortalFunction##IT);
  ImmortalFunctions(V);
#undef V
}

}  // namespace process
}  // namespace aworker

AWORKER_BINDING_REGISTER(process,
                         aworker::process::Init,
                         aworker::process::Init)
