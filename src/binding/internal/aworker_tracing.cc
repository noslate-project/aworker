#include <cmath>

#include "aworker_binding.h"
#include "error_handling.h"
#include "handle_wrap.h"
#include "immortal.h"
#include "util.h"
#include "uv.h"

namespace aworker {
namespace tracing {

using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;

AWORKER_BINDING(Init) {
  Isolate* isolate = immortal->isolate();

  Local<String> isTraceCategoryEnabled =
      OneByteString(isolate, "isTraceCategoryEnabled");
  Local<String> trace = OneByteString(isolate, "trace");

  // Grab the trace and isTraceCategoryEnabled intrinsics from the binding
  // object and expose those to our binding layer.
  Local<Object> binding = context->GetExtrasBindingObject();
  exports
      ->Set(context,
            isTraceCategoryEnabled,
            binding->Get(context, isTraceCategoryEnabled).ToLocalChecked())
      .Check();
  exports->Set(context, trace, binding->Get(context, trace).ToLocalChecked())
      .Check();
}

AWORKER_EXTERNAL_REFERENCE(Init) {}

}  // namespace tracing
}  // namespace aworker

AWORKER_BINDING_REGISTER(tracing,
                         aworker::tracing::Init,
                         aworker::tracing::Init)
