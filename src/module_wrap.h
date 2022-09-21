#pragma once

#include <unordered_map>
#include "aworker_binding.h"
#include "base_object.h"
#include "util.h"
#include "v8.h"

namespace aworker {

class ModuleWrap : public BaseObject {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static v8::MaybeLocal<v8::Promise>
  HostImportModuleDynamicallyWithImportAssertionsCallback(
      v8::Local<v8::Context> context,
      v8::Local<v8::ScriptOrModule> referrer,
      v8::Local<v8::String> specifier,
      v8::Local<v8::FixedArray> import_assertions);
  static void HostInitializeImportMetaObjectCallback(
      v8::Local<v8::Context> context,
      v8::Local<v8::Module> module,
      v8::Local<v8::Object> meta);
  static void Init(Immortal* immortal, v8::Local<v8::Object> exports);
  static void Init(ExternalReferenceRegistry* registry);

  inline v8::Local<v8::Module> module() {
    return PersistentToLocal::Strong<v8::Module>(_module);
  }

  ~ModuleWrap() override;

 private:
  ModuleWrap(Immortal* immortal,
             v8::Local<v8::Object> object,
             v8::Local<v8::Module> module,
             v8::Local<v8::String> url);

  static v8::MaybeLocal<v8::Module> ResolveCallback(
      v8::Local<v8::Context> context,
      v8::Local<v8::String> specifier,
      v8::Local<v8::Module> referrer);
  static ModuleWrap* GetFromModule(Immortal*, v8::Local<v8::Module>);

  static AWORKER_METHOD(New);
  static AWORKER_METHOD(Link);
  static AWORKER_METHOD(Instantiate);

  static AWORKER_METHOD(GetNamespace);

  static AWORKER_METHOD(SetImportModuleDynamicallyCallback);
  static AWORKER_METHOD(SetInitializeImportMetaObjectCallback);

  v8::Global<v8::Module> _module;
  std::unordered_map<std::string, v8::Global<v8::Promise>> _resolve_cache;
  bool _linked = false;
};

}  // namespace aworker
