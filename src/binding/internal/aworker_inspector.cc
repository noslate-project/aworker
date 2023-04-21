#include "aworker_binding.h"
#include "error_handling.h"
#include "immortal-inl.h"
#include "inspector/inspector_agent.h"

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Value;

namespace aworker {
namespace inspector {

void CallAndPauseOnFirstStatement(const FunctionCallbackInfo<Value>& info) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  if (info.Length() < 2) {
    return THROW_ERR_INVALID_ARGUMENT(immortal, "Expect more than 1 argument");
  }
  if (!info[0]->IsFunction()) {
    return THROW_ERR_FUNCTION_EXPECTED(immortal,
                                       "First argument must be a function");
  }
  SlicedArguments call_args(info, /* start */ 2);
  if (!immortal->inspector_agent()->IsActive()) {
    return THROW_ERR_INSPECTOR_NOT_ACTIVE(immortal, "Inspector is not active");
  }
  if (!immortal->inspector_agent()->HasConnectedSessions()) {
    immortal->inspector_agent()->WaitForConnect();
  }
  immortal->inspector_agent()->PauseOnNextJavascriptStatement("Break on call");
  MaybeLocal<Value> retval = info[0].As<Function>()->Call(
      immortal->context(), info[1], call_args.size(), call_args.data());
  if (!retval.IsEmpty()) {
    info.GetReturnValue().Set(retval.ToLocalChecked());
  }
}

void IsInspectorActive(const FunctionCallbackInfo<Value>& info) {
  Immortal* immortal = Immortal::GetCurrent(info);
  info.GetReturnValue().Set(immortal->inspector_agent()->IsActive());
}

AWORKER_BINDING(Init) {
  immortal->SetFunctionProperty(exports, "isActive", IsInspectorActive);
  immortal->SetFunctionProperty(
      exports, "callAndPauseOnFirstStatement", CallAndPauseOnFirstStatement);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  registry->Register(IsInspectorActive);
  registry->Register(CallAndPauseOnFirstStatement);
}

}  // namespace inspector
}  // namespace aworker

AWORKER_BINDING_REGISTER(inspector,
                         aworker::inspector::Init,
                         aworker::inspector::Init)
