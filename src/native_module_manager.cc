#include "native_module_manager.h"
#include "debug_utils.h"
#include "error_handling.h"
#include "immortal.h"
#include "util.h"

namespace aworker {

using std::map;
using std::string;
using std::vector;

using v8::Context;
using v8::EscapableHandleScope;
using v8::Function;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::String;
using v8::True;
using v8::TryCatch;

NativeModuleManager::NativeModuleManager() : Singleton<NativeModuleManager>() {
  InitializeNativeModuleSource();
  InitializeNativeModuleTypes();
}

void NativeModuleManager::InitializeNativeModuleTypes() {
  native_module_types_["bootstrap/loader.js"] = NativeModuleType::Loader;

  std::string bootstrap_prefix = "bootstrap/";
  for (auto it = native_module_source_map_.cbegin();
       it != native_module_source_map_.cend();
       it++) {
    if (native_module_types_.find(it->first) != native_module_types_.cend()) {
      continue;
    }
    if (it->first.find(bootstrap_prefix) == 0) {
      native_module_types_[it->first] = NativeModuleType::Bootstrapping;
    } else {
      native_module_types_[it->first] = NativeModuleType::Module;
    }
  }
}

void NativeModuleManager::InitializeCachedData(
    const std::vector<NativeModuleCache>& caches) {
  for (auto const& it : caches) {
    size_t length = it.data.size();
    uint8_t* buffer = new uint8_t[length];
    memcpy(buffer, it.data.data(), length);
    auto new_cache = std::make_unique<v8::ScriptCompiler::CachedData>(
        buffer, length, v8::ScriptCompiler::CachedData::BufferOwned);
    auto cache_it = cached_data_map_.find(it.id);
    if (cache_it != cached_data_map_.end()) {
      // Release the old cache and replace it with the new copy.
      cache_it->second = std::move(new_cache);
    } else {
      cached_data_map_.emplace(it.id, std::move(new_cache));
    }
    per_process::Debug(DebugCategory::NATIVE_MODULE,
                       "Initialized native module (%s) cached data\n",
                       it.id.c_str());
  }
}

std::vector<NativeModuleCache> NativeModuleManager::GetNativeModuleCaches()
    const {
  std::vector<NativeModuleCache> out;
  for (auto const& it : cached_data_map_) {
    out.push_back(
        {it.first, {it.second->data, it.second->data + it.second->length}});
  }
  return out;
}

vector<string> NativeModuleManager::GetModuleIds() {
  vector<string> ids;
  for (auto it = native_module_source_map_.cbegin();
       it != native_module_source_map_.cend();
       it++) {
    ids.push_back(it->first);
  }
  return ids;
}

ScriptCompiler::CachedData* NativeModuleManager::GetCachedData(std::string id) {
  auto it = cached_data_map_.find(id);
  if (it == cached_data_map_.cend()) {
    return nullptr;
  }
  return it->second.get();
}

bool NativeModuleManager::CompileAll(Local<Context> context) {
  bool all_succeeded = true;
  NativeModuleResult result;
  TryCatch compile_scope(context->GetIsolate());
  for (auto const& it : GetModuleIds()) {
    MaybeLocal<Function> fn = Compile(context, it, &result);
    if (fn.IsEmpty()) {
      all_succeeded = false;
      CHECK(compile_scope.HasCaught());
      fprintf(stderr, "Failed to compile native module %s:\n", it.c_str());
      PrintCaughtException(context, compile_scope);
    }
  }
  return all_succeeded;
}

MaybeLocal<Function> NativeModuleManager::Compile(Local<Context> context,
                                                  std::string id,
                                                  NativeModuleResult* result) {
  NativeModuleType type;
  auto found = native_module_types_.find(id);
  if (found != native_module_types_.cend()) {
    type = found->second;
  } else {
    DCHECK(false);
    type = NativeModuleType::Module;
  }
  result->type = type;

  auto isolate = context->GetIsolate();
  vector<Local<String>> parameters;
  switch (type) {
    case NativeModuleType::Module: {
      parameters = {OneByteString(isolate, "wrapper"),
                    OneByteString(isolate, "mod"),
                    OneByteString(isolate, "load"),
                    OneByteString(isolate, "loadBinding")};
      break;
    }
    case NativeModuleType::Bootstrapping: {
      parameters = {OneByteString(isolate, "load"),
                    OneByteString(isolate, "loadBinding")};
      break;
    }
    case NativeModuleType::Loader: {
      parameters = {OneByteString(isolate, "getNativeModule"),
                    OneByteString(isolate, "getBinding")};
      break;
    }
  }
  return Compile(context, id, parameters, result);
}

MaybeLocal<Function> NativeModuleManager::Compile(
    Local<Context> context,
    std::string id,
    vector<Local<String>> parameters,
    NativeModuleResult* result) {
  Isolate* isolate = context->GetIsolate();

  EscapableHandleScope scope(isolate);

  std::string filename = "internal:" + id;
  Local<String> v8_filename = OneByteString(isolate, filename.c_str());
  Local<String> source = GetModuleSource(isolate, id);

  ScriptOrigin origin(isolate, v8_filename, 0, 0, true);

  ScriptCompiler::CachedData* cached_data = nullptr;
  auto cache_it = cached_data_map_.find(id);
  if (cache_it != cached_data_map_.end()) {
    // Transfer ownership to ScriptCompiler::Source later.
    cached_data = cache_it->second.release();
    cached_data_map_.erase(cache_it);
  }

  const bool has_cache = cached_data != nullptr;
  ScriptCompiler::CompileOptions options =
      has_cache ? ScriptCompiler::kConsumeCodeCache
                : ScriptCompiler::kEagerCompile;
  ScriptCompiler::Source script_source(source, origin, cached_data);

  MaybeLocal<Function> maybe_function =
      ScriptCompiler::CompileFunctionInContext(context,
                                               &script_source,
                                               parameters.size(),
                                               parameters.data(),
                                               0,
                                               nullptr,
                                               options);
  Local<Function> func;
  if (!maybe_function.ToLocal(&func)) {
    return MaybeLocal<Function>();
  }

  result->with_cache =
      (has_cache && !script_source.GetCachedData()->rejected) ? true : false;

  per_process::Debug(
      DebugCategory::NATIVE_MODULE,
      "Load native module (%s, has_cache %d) with code cache? %d\n",
      id,
      has_cache,
      result->with_cache);

  // Generate new cache for next compilation
  std::unique_ptr<ScriptCompiler::CachedData> new_cached_data(
      ScriptCompiler::CreateCodeCacheForFunction(func));
  CHECK_NOT_NULL(new_cached_data);
  cached_data_map_.emplace(id, std::move(new_cached_data));

  return scope.Escape(func);
}

Local<String> NativeModuleManager::GetModuleSource(Isolate* isolate,
                                                   string id) {
  const uint8_t* source = native_module_source_map_[id];
  if (source == nullptr) {
    fprintf(stderr, "Native Module id (%s) not found\n", id.c_str());
    CHECK_NE(source, nullptr);
  }

  MaybeLocal<String> maybe_local_result =
      String::NewFromOneByte(isolate, source);
  CHECK(!maybe_local_result.IsEmpty());
  Local<String> result = maybe_local_result.ToLocalChecked();

  return result;
}

// static
MaybeLocal<Function> NativeModuleManager::Compile(Immortal* immortal,
                                                  std::string path,
                                                  NativeModuleResult* result) {
  MaybeLocal<Function> maybe_func = NativeModuleManager::Instance().Compile(
      immortal->context(), path, result);
  if (maybe_func.IsEmpty()) {
    return maybe_func;
  }

  if (result->with_cache) {
    immortal->loaded_native_modules_with_cache.insert(path);
  } else {
    immortal->loaded_native_modules_without_cache.insert(path);
  }
  return maybe_func;
}

// static
AWORKER_METHOD(NativeModuleManager::GetNativeModuleFunction) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();

  HandleScope scope(isolate);

  if (info.Length() < 1 || !info[0]->IsString()) {
    ThrowException(isolate,
                   "getNativeModuleFunction arguments error.",
                   ExceptionType::kTypeError);
    return;
  }

  Local<String> path = info[0].As<String>();
  String::Utf8Value path_utf8value(isolate, path);

  NativeModuleResult result;
  MaybeLocal<Function> maybe_func =
      NativeModuleManager::Compile(immortal, *path_utf8value, &result);
  DCHECK_EQ(result.type, NativeModuleType::Module);

  Local<Function> func;
  if (!maybe_func.ToLocal(&func)) return;
  info.GetReturnValue().Set(func);
}

}  // namespace aworker
