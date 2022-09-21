#ifndef SRC_CONTEXTIFY_CONTEXT_H_
#define SRC_CONTEXTIFY_CONTEXT_H_
#include "aworker_binding.h"
#include "base_object.h"
#include "error_handling.h"
#include "immortal.h"
#include "module_wrap.h"

namespace aworker {
namespace contextify {

class ScriptWrap;

AWORKER_BINDING(ContextInit);
AWORKER_EXTERNAL_REFERENCE(ContextInit);

enum ExecutionFlags {
  kNone = 0,
  kBreakOnFirstLine = 1 << 0,
  kModule = 1 << 1,
};

class ContextWrap : public BaseObject {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ContextWrap(Immortal* immortal,
              v8::Local<v8::Object> object,
              v8::Local<v8::Object> sandbox,
              const std::string& name,
              const std::string& origin);
  ContextWrap(Immortal* immortal,
              v8::Local<v8::Object> object,
              v8::Local<v8::Context> context);
  ~ContextWrap();

  void MakeWeak();

  v8::Local<v8::Context> CreateV8Context(v8::Local<v8::Object> sandbox,
                                         const std::string& name,
                                         const std::string& origin);

  inline v8::Local<v8::Context> context() const {
    return PersistentToLocal::Default(isolate(), context_);
  }

  inline v8::Local<v8::Object> sandbox() const {
    return v8::Local<v8::Object>::Cast(
        context()->GetEmbedderData(ContextEmbedderIndex::kSandboxObject));
  }

  inline v8::Local<v8::Object> global() const { return context()->Global(); }

 private:
  v8::Global<v8::Context> context_;

 public:
  template <typename T>
  static ContextWrap* Get(const v8::PropertyCallbackInfo<T>& args) {
    v8::Local<v8::Context> context;
    if (!args.This()->GetCreationContext().ToLocal(&context)) {
      return nullptr;
    }
    if (UNLIKELY(!ContextEmbedderTag::IsContextValid(context))) {
      return nullptr;
    }

    return static_cast<ContextWrap*>(context->GetAlignedPointerFromEmbedderData(
        ContextEmbedderIndex::kContextifyContext));
  }

  static void GlobalPropertyGetterCallback(
      v8::Local<v8::Name> property,
      const v8::PropertyCallbackInfo<v8::Value>& args);

  static void GlobalPropertySetterCallback(
      v8::Local<v8::Name> property,
      v8::Local<v8::Value> value,
      const v8::PropertyCallbackInfo<v8::Value>& args);

  static void GlobalPropertyDescriptorCallback(
      v8::Local<v8::Name> property,
      const v8::PropertyCallbackInfo<v8::Value>& args);

  static void GlobalPropertyDefinerCallback(
      v8::Local<v8::Name> property,
      const v8::PropertyDescriptor& desc,
      const v8::PropertyCallbackInfo<v8::Value>& args);

  static void GlobalPropertyQueryCallback(
      v8::Local<v8::Name> property,
      const v8::PropertyCallbackInfo<v8::Integer>& args);

  static void GlobalPropertyDeleterCallback(
      v8::Local<v8::Name> property,
      const v8::PropertyCallbackInfo<v8::Boolean>& args);

  static void GlobalPropertyEnumeratorCallback(
      const v8::PropertyCallbackInfo<v8::Array>& args);

  static v8::MaybeLocal<v8::Value> DoExecute(Immortal* immortal,
                                             ScriptWrap* script,
                                             const int64_t timeout,
                                             const ExecutionFlags flag);
  static v8::MaybeLocal<v8::Value> DoExecute(Immortal* immortal,
                                             ModuleWrap* module,
                                             const int64_t timeout,
                                             const ExecutionFlags flag);

  static void AlarmHandler(int sig);
  static void DecorateErrorStack(Immortal* immortal,
                                 const v8::TryCatch& try_catch);

 public:
  static AWORKER_METHOD(New);
  static AWORKER_METHOD(GetGlobalThis);
  static AWORKER_METHOD(Execute);
};

}  // namespace contextify
}  // namespace aworker

#endif  // SRC_CONTEXTIFY_CONTEXT_H_
