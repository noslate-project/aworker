#ifndef SRC_NATIVE_MODULE_MANAGER_H_
#define SRC_NATIVE_MODULE_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "aworker_binding.h"
#include "snapshot/snapshot_builder.h"
#include "util.h"
#include "utils/singleton.h"
#include "v8.h"

namespace aworker {

class Immortal;

enum class NativeModuleType {
  Loader,        /** Modules with parameters as (loadBinding) */
  Bootstrapping, /** Modules with parameters as (load, loadBinding) */
  Module, /** Modules with parameters as (wrapper, mod, load, loadBinding) */
};

struct NativeModuleResult {
  NativeModuleType type;
  bool with_cache;
};

class NativeModuleManager : public Singleton<NativeModuleManager> {
 public:
  void InitializeCachedData(
      const std::vector<NativeModuleCache>& native_module_caches);
  std::vector<NativeModuleCache> GetNativeModuleCaches() const;

  std::vector<std::string> GetModuleIds();
  v8::ScriptCompiler::CachedData* GetCachedData(std::string id);
  bool CompileAll(v8::Local<v8::Context> context);

  static v8::MaybeLocal<v8::Function> Compile(Immortal* immortal,
                                              std::string path,
                                              NativeModuleResult* result);

  static AWORKER_METHOD(GetNativeModuleFunction);

 private:
  NativeModuleManager();

  void InitializeNativeModuleSource();
  void InitializeNativeModuleTypes();

  v8::MaybeLocal<v8::Function> Compile(v8::Local<v8::Context> context,
                                       std::string id,
                                       NativeModuleResult* result);
  v8::MaybeLocal<v8::Function> Compile(
      v8::Local<v8::Context> context,
      std::string id,
      std::vector<v8::Local<v8::String>> parameters,
      NativeModuleResult* result);

  v8::Local<v8::String> GetModuleSource(v8::Isolate* isolate, std::string id);

  std::map<std::string, const uint8_t*> native_module_source_map_;
  std::map<std::string, NativeModuleType> native_module_types_;
  std::map<std::string, std::unique_ptr<v8::ScriptCompiler::CachedData>>
      cached_data_map_;

  /** Initialization */
  friend Singleton;
};

}  // namespace aworker

#endif  // SRC_NATIVE_MODULE_MANAGER_H_
