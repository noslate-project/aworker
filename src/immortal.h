#ifndef SRC_IMMORTAL_H_
#define SRC_IMMORTAL_H_

#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "macro_task_queue.h"
#include "memory_tracker.h"
#include "util.h"
#include "utils/async_primitives.h"
#include "utils/singleton.h"
#include "uv.h"
#include "v8.h"

namespace aworker {

/** MARK: - Forward declarations */
class CommandlineParserGroup;
namespace cache {
class CacheStorage;
}
class CallbackScope;

class AgentDataChannel;
class AgentDiagChannel;

namespace inspector {
class InspectorAgent;
}

namespace binding {
struct Binding;
}

namespace report {
class ReportWatchdog;
}

class BaseObject;
class ModuleWrap;
class HandleWrap;
class Watchdog;
class LoopLatencyWatchdog;
/** MARK: END - Forward declarations */

struct ContextInfo {
  std::string name;
  std::string origin;
  bool is_default = false;
};

enum class WorkerState {
  kBootstrapping = 0,
  kSerialized = 1,
  kDeserialized = 2,
  kForkPrepared = 3,
  kInstalled = 4,
  kActivated = 5,
};

enum class StartupForkMode {
  kNone = 0b00,
  kFork = 0b01,
  kForkUserland = 0b11,
};

// kContextTag should be the last item.
enum ContextEmbedderIndex {
  kImmortal = 1,
  kContextifyContext,
  kSandboxObject,
  kJavaScriptContextObject,
  kContextTag,
};

class ContextEmbedderTag {
 public:
  static void TagContext(v8::Local<v8::Context> context);
  static bool IsContextValid(v8::Local<v8::Context> context);

 private:
  static void* const kContextEmbedderTagPtr;
  static int const kContextEmbedderTag;
};

class KVStore {
 public:
  KVStore() = default;
  virtual ~KVStore() = default;
  KVStore(const KVStore&) = delete;
  KVStore& operator=(const KVStore&) = delete;
  KVStore(KVStore&&) = delete;
  KVStore& operator=(KVStore&&) = delete;

  virtual v8::MaybeLocal<v8::String> Get(v8::Isolate* isolate,
                                         v8::Local<v8::String> key) const = 0;
  virtual v8::Maybe<std::string> Get(const char* key) const = 0;
  virtual void Set(v8::Isolate* isolate,
                   v8::Local<v8::String> key,
                   v8::Local<v8::String> value) = 0;
  virtual int32_t Query(v8::Isolate* isolate,
                        v8::Local<v8::String> key) const = 0;
  virtual int32_t Query(const char* key) const = 0;
  virtual void Delete(v8::Isolate* isolate, v8::Local<v8::String> key) = 0;
  virtual v8::Local<v8::Array> Enumerate(v8::Isolate* isolate) const = 0;
};

#define IMMORTAL_NORMAL_PROPERTIES(V)                                          \
  V(WorkerState, worker_state)                                                 \
  V(StartupForkMode, startup_fork_mode)                                        \
  V(v8::Isolate*, isolate)                                                     \
  V(std::shared_ptr<AgentDataChannel>, agent_data_channel)                     \
  V(std::shared_ptr<AgentDiagChannel>, agent_diag_channel)                     \
  V(std::shared_ptr<inspector::InspectorAgent>, inspector_agent)               \
  V(CommandlineParserGroup*, commandline_parser)                               \
  V(uv_loop_t*, event_loop)                                                    \
  V(std::shared_ptr<KVStore>, env_vars)                                        \
  V(cache::CacheStorage*, cache_storage)                                       \
  V(CallbackScope*, callback_scope_stack_top)                                  \
  V(std::shared_ptr<MacroTaskQueue>, macro_task_queue)

#define IMMORTAL_GLOBAL_PROPERTIES(V)                                          \
  V(v8::FunctionTemplate, base_object_ctor_template)                           \
  V(v8::FunctionTemplate, async_wrap_ctor_template)                            \
  V(v8::FunctionTemplate, handle_wrap_ctor_template)                           \
  V(v8::Context, context)                                                      \
  V(v8::Object, process_object)                                                \
  V(v8::Function, loader_function)                                             \
  V(v8::Function, load_binding_function)                                       \
  V(v8::Function, agent_channel_callback)                                      \
  V(v8::Function, agent_channel_handler)                                       \
  V(v8::Function, async_wrap_init_function)                                    \
  V(v8::Function, async_wrap_before_function)                                  \
  V(v8::Function, async_wrap_after_function)                                   \
  V(v8::Function, dom_exception_constructor)                                   \
  V(v8::Function, module_wrap_dynamic_import_function)                         \
  V(v8::Function, module_wrap_init_import_meta_function)                       \
  V(v8::Function, promise_init_hook)                                           \
  V(v8::Function, promise_before_hook)                                         \
  V(v8::Function, promise_after_hook)                                          \
  V(v8::Function, promise_resolve_hook)

#define PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(V)                               \
  V(contextify_global_private, "aworker:contextify:global")

#define PER_ISOLATE_STRING_PROPERTIES(V)                                       \
  V(emit_promise_rejection, "emitPromiseRejection")                            \
  V(emit_uncaught_exception, "emitUncaughtException")                          \
  V(handle_onclose, "handle_onclose")                                          \
  V(level, "level")                                                            \
  V(mem_level, "memLevel")                                                     \
  V(message, "message")                                                        \
  V(name, "name")                                                              \
  V(onimmediate, "onimmediate")                                                \
  V(ontimeout, "ontimeout")                                                    \
  V(on_signal, "onSignal")                                                     \
  V(openssl_error_stack, "opensslErrorStack")                                  \
  V(prepare_stack_trace, "prepareStackTrace")                                  \
  V(run_cleanup_hooks, "runCleanupHooks")                                      \
  V(stack, "stack")                                                            \
  V(strategy, "strategy")                                                      \
  V(tick_task_queue, "tickTaskQueue")

#define IMMORTAL_DECLARE_PROPERTY(inner_type, exchange_type, name)             \
 public:                                                                       \
  inline void set_##name(exchange_type value);                                 \
  inline exchange_type name();                                                 \
                                                                               \
 private:                                                                      \
  inner_type name##_;

/**
 * IsolateData represents a data structure that is fully serializable into v8
 * snapshots. Native data or native references should not present in the
 * IsolateData.
 */
class IsolateData {
 public:
  IsolateData(v8::Isolate* isolate,
              const std::vector<size_t>* indexes = nullptr);
  AWORKER_DISALLOW_ASSIGN_COPY(IsolateData)

  std::vector<size_t> Serialize(v8::SnapshotCreator* creator);

#define V(type, name)                                                          \
  IMMORTAL_DECLARE_PROPERTY(v8::Global<type>, v8::Local<type>, name)
#define VP(name, value) V(v8::Private, name##_symbol)
#define VS(name, value) V(v8::String, name##_string)
  PER_ISOLATE_STRING_PROPERTIES(VS);
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP);
#undef VS
#undef VP
#undef V

 private:
  void DeserializeProperties(const std::vector<size_t>* indexes);
  void CreateProperties();

  v8::Isolate* isolate_;
};

class Immortal : public MemoryRetainer {
 public:
  using V8RawFunction = void (*)(const v8::FunctionCallbackInfo<v8::Value>&);
  using V8RawGetter = void (*)(v8::Local<v8::Name>,
                               const v8::PropertyCallbackInfo<v8::Value>&);
  using V8RawSetter = void (*)(v8::Local<v8::Name>,
                               v8::Local<v8::Value>,
                               const v8::PropertyCallbackInfo<void>&);

 private:
#define VN(type, name) IMMORTAL_DECLARE_PROPERTY(type, type, name)
#define VG(type, name)                                                         \
  IMMORTAL_DECLARE_PROPERTY(v8::Global<type>, v8::Local<type>, name)
  IMMORTAL_NORMAL_PROPERTIES(VN)
  IMMORTAL_GLOBAL_PROPERTIES(VG)
#undef VN
#undef VG

 public:
#define V(type, name) inline v8::Local<type> name();
#define VP(name, value) V(v8::Private, name##_symbol)
#define VS(name, value) V(v8::String, name##_string)
  PER_ISOLATE_STRING_PROPERTIES(VS)
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
#undef VS
#undef VP
#undef V

 public:
  static inline Immortal* GetCurrent(v8::Isolate* isolate);
  static inline Immortal* GetCurrent(v8::Local<v8::Context> context);
  static inline Immortal* GetCurrent(
      const v8::FunctionCallbackInfo<v8::Value>& info);
  template <typename T>
  static inline Immortal* GetCurrent(const v8::PropertyCallbackInfo<T>& info);
  static void BuildEmbedderGraph(v8::Isolate* isolate,
                                 v8::EmbedderGraph* graph,
                                 void* data);

  Immortal(uv_loop_t* loop,
           CommandlineParserGroup* options,
           v8::Isolate* isolate,
           IsolateData* isolate_data,
           const std::vector<size_t>* indexes);
  ~Immortal();

  MEMORY_INFO_NAME(Immortal)
  SIZE_IN_BYTES(Immortal)
  void MemoryInfo(MemoryTracker* tracker) const override;

  std::vector<size_t> Serialize(v8::SnapshotCreator* creator);
  void Deserialize(const std::vector<size_t>* indexes);

  /**
   * Bootstrap JavaScript environments that are capable of fork/snapshotting.
   */
  bool Bootstrap();
  /**
   * Bootstrap JavaScript environments that are specific to per execution.
   */
  void BootstrapPerExecution();
  /**
   * Bootstrap agent connections.
   */
  void BootstrapAgent(const std::string& script_filename);
  void StopAgent();
  /**
   * Start user code execution.
   */
  v8::Maybe<bool> StartForkExecution();
  /**
   * Start user code execution.
   */
  v8::Maybe<bool> StartExecution();

  v8::Maybe<bool> RunCleanupHooks();

  void Exit(int code);

  void Raise(int sig_num);

  void StartLoopLatencyWatchdog();

  // TODO(chengzhong.wcz): Move helper funtions out of Immortal.
  bool SetAccessor(v8::Local<v8::Object> object,
                   const char* name,
                   V8RawGetter getter,
                   V8RawSetter setter = nullptr);
  bool SetAccessor(v8::Local<v8::ObjectTemplate> tpl,
                   const char* name,
                   V8RawGetter getter,
                   V8RawSetter setter = nullptr);
  bool SetFunctionProperty(v8::Local<v8::Object> object,
                           const char* name,
                           V8RawFunction func);
  bool SetFunctionProperty(v8::Local<v8::ObjectTemplate> tpl,
                           const char* name,
                           V8RawFunction func);
  bool SetIntegerProperty(v8::Local<v8::Object> object,
                          const char* name,
                          int num);
  bool SetValueProperty(v8::Local<v8::Object> object,
                        const char* name,
                        v8::Local<v8::Value> value);

  void RequestInterrupt(std::function<void()>&& callback);
  void ExhaustInterruptRequests();

  inline void AssignToContext(v8::Local<v8::Context> context);

  inline v8::Local<v8::Object> global_object();

  inline Watchdog* watchdog() const {
    return watchdog_.get();
  }

  std::set<binding::Binding*> loaded_internal_bindings;
  std::set<std::string> loaded_native_modules_with_cache;
  std::set<std::string> loaded_native_modules_without_cache;
  std::unordered_map<int, ModuleWrap*> hash_to_module_map;
  std::set<BaseObject*> tracked_base_objects;
  std::set<HandleWrap*> tracked_handle_wraps;

 private:
  static void OnPrepare(uv_prepare_t* handle);
  static void OnCheck(uv_check_t* handle);

  /** bootstraps that can be forked/snapshotted */
  void InitializeIsolate();
  void InitializeDefaultContext();
  bool BootstrapLoader();
  bool BootstrapPureNativeModules();

  /** Per execution bootstrapping */
  void StartAgentChannel();
  void BootstrapContextPerExecution();

  v8::Maybe<bool> Evaluate(const char* module);

  IsolateData* isolate_data_;
  UvAsync<std::function<void()>>::Ptr interrupt_async_;
  std::list<std::function<void()>> interrupt_requests_;
  std::mutex interrupt_mutex_;
  std::unique_ptr<Watchdog> watchdog_;
  std::unique_ptr<report::ReportWatchdog> report_watchdog_;
  std::unique_ptr<LoopLatencyWatchdog> loop_latency_watchdog_;

  uv_prepare_t prepare_handle_;
  uv_check_t check_handle_;
};

#undef IMMORTAL_DECLARE_PROPERTY

}  // namespace aworker

#include "immortal-inl.h"

#endif  // SRC_IMMORTAL_H_
