#include "aworker_binding.h"
#include <cstring>
#include <list>
#include <vector>
#include "debug_utils.h"
#include "immortal.h"
#include "util.h"

#define BUILTIN_BINDINGS(V)                                                    \
  V(noslated_data_channel)                                                        \
  V(async_wrap)                                                                \
  V(bytes)                                                                     \
  V(cache)                                                                     \
  V(constants)                                                                 \
  V(contextify)                                                                \
  V(crypto)                                                                    \
  V(curl)                                                                      \
  V(errors)                                                                    \
  V(file)                                                                      \
  V(i18n)                                                                      \
  V(module_wrap)                                                               \
  V(perf)                                                                      \
  V(process)                                                                   \
  V(serdes)                                                                    \
  V(aworker_options)                                                           \
  V(task_queue)                                                                \
  V(timers)                                                                    \
  V(tracing)                                                                   \
  V(types)                                                                     \
  V(url)                                                                       \
  V(v8)                                                                        \
  V(zlib)

#define V(name) void _register_##name();
BUILTIN_BINDINGS(V)
#undef V

void RegisterBindings() {
#define V(name) _register_##name();
  BUILTIN_BINDINGS(V)
#undef V
}

namespace aworker {
namespace per_process {
std::list<aworker::binding::Binding*> bindings;
}

namespace binding {

using std::vector;

using v8::Context;
using v8::HandleScope;
using v8::Local;
using v8::Object;
using v8::String;

Binding::Binding(const char* name,
                 const BindingRegisterFunction func,
                 const BindingExternalReferenceFunction external_reference_func)
    : name(name),
      func(func),
      external_reference_func(external_reference_func) {}

AWORKER_METHOD(GetBinding) {
  auto immortal = Immortal::GetCurrent(info);
  auto isolate = immortal->isolate();
  HandleScope scope(isolate);

  CHECK(info[0]->IsString());
  CHECK(info[1]->IsObject());

  Local<Context> context = immortal->context();
  Local<String> name = info[0].As<String>();
  Local<Object> exports = info[1].As<Object>();
  String::Utf8Value name_utf8v(isolate, name);

  Binding* found = nullptr;

  for (auto it : per_process::bindings) {
    if (std::strcmp(it->name, *name_utf8v) == 0) {
      found = it;
      break;
    }
  }

  if (found == nullptr) {
    FPrintF(stderr, "No such binding: %s\n", *name_utf8v);
    CHECK_NOT_NULL(found);
  }

  // Calling into binding;
  found->func(exports, context, immortal);

  immortal->loaded_internal_bindings.insert(found);

  info.GetReturnValue().Set(exports);
}

}  // namespace binding
}  // namespace aworker
