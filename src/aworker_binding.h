#ifndef SRC_AWORKER_BINDING_H_
#define SRC_AWORKER_BINDING_H_

#include <list>
#include <string>
#include "external_references.h"
#include "v8.h"

void RegisterBindings();
namespace aworker {

class Immortal;

namespace binding {
#define AWORKER_METHOD(name)                                                   \
  void name(const v8::FunctionCallbackInfo<v8::Value>& info)

#define AWORKER_GETTER(name)                                                   \
  void name(v8::Local<v8::Name> _,                                             \
            const v8::PropertyCallbackInfo<v8::Value>& info)

#define AWORKER_SETTER(name)                                                   \
  void name(v8::Local<v8::Name> _,                                             \
            v8::Local<v8::Value> value,                                        \
            const v8::PropertyCallbackInfo<void>& info)

#define AWORKER_BINDING(name)                                                  \
  void name(v8::Local<v8::Object> exports,                                     \
            v8::Local<v8::Context> context,                                    \
            Immortal* immortal)

#define AWORKER_EXTERNAL_REFERENCE(name)                                       \
  void name(ExternalReferenceRegistry* registry)

struct Binding {
  using BindingRegisterFunction = void (*)(v8::Local<v8::Object>,
                                           v8::Local<v8::Context>,
                                           Immortal* immortal);
  using BindingExternalReferenceFunction =
      void (*)(ExternalReferenceRegistry* registry);

 public:
  Binding(const char* name,
          const BindingRegisterFunction func,
          const BindingExternalReferenceFunction external_reference_func);
  const char* name;
  const BindingRegisterFunction func;
  const BindingExternalReferenceFunction external_reference_func;
};

AWORKER_METHOD(GetBinding);

#define AWORKER_BINDING_REGISTER(name, register_func, external_reference_func) \
  ::aworker::binding::Binding binding_##name(                                  \
      #name, register_func, external_reference_func);                          \
  void _register_##name() {                                                    \
    aworker::per_process::bindings.push_back(&binding_##name);                 \
  }

}  // namespace binding
namespace per_process {
extern std::list<aworker::binding::Binding*> bindings;
}
}  // namespace aworker

#endif  // SRC_AWORKER_BINDING_H_
