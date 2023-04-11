#include "immortal.h"
#include "agent_channel/noslated_data_channel.h"
#include "agent_channel/noslated_diag_channel.h"
#include "aworker.h"
#include "aworker_binding.h"
#include "binding/internal/aworker_cache.h"
#include "binding/internal/process.h"
#include "command_parser_group.h"
#include "debug_utils.h"
#include "diag_report.h"
#include "error_handling.h"
#include "external_references.h"
#include "handle_wrap.h"
#include "inspector/inspector_agent.h"
#include "loop_latency_watchdog.h"
#include "module_wrap.h"
#include "native_module_manager.h"
#include "snapshot/snapshot_builder.h"
#include "snapshot/snapshotable.h"
#include "task_queue.h"
#include "tracing/trace_event.h"
#include "util.h"
#include "utils/async_primitives.h"
#include "v8-profiler.h"

namespace aworker {

using std::vector;

using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Object;
using v8::ObjectTemplate;
using v8::Private;
using v8::Script;
using v8::String;
using v8::TryCatch;
using v8::Undefined;
using v8::Value;

/** code for snk */
int const ContextEmbedderTag::kContextEmbedderTag = 0x736e6b;
void* const ContextEmbedderTag::kContextEmbedderTagPtr = const_cast<void*>(
    static_cast<const void*>(&ContextEmbedderTag::kContextEmbedderTag));

// static
void ContextEmbedderTag::TagContext(v8::Local<v8::Context> context) {
  // Used by Immortal::GetCurrent to know that we are on a aworker context.
  context->SetAlignedPointerInEmbedderData(
      ContextEmbedderIndex::kContextTag,
      ContextEmbedderTag::kContextEmbedderTagPtr);
}

// static
bool ContextEmbedderTag::IsContextValid(v8::Local<v8::Context> context) {
  if (UNLIKELY(context.IsEmpty())) {
    return false;
  }
  if (UNLIKELY(context->GetNumberOfEmbedderDataFields() <=
               ContextEmbedderIndex::kContextTag)) {
    return false;
  }
  if (UNLIKELY(context->GetAlignedPointerFromEmbedderData(
                   ContextEmbedderIndex::kContextTag) !=
               ContextEmbedderTag::kContextEmbedderTagPtr)) {
    return false;
  }
  return true;
}

MaybeLocal<Value> PrepareStackTraceCallback(Local<Context> context,
                                            Local<Value> error,
                                            Local<Array> sites) {
  Immortal* immortal = Immortal::GetCurrent(context);
  Isolate* isolate = immortal->isolate();

  MaybeLocal<Value> prepare = immortal->process_object()->Get(
      context, immortal->prepare_stack_trace_string());
  if (prepare.IsEmpty()) {
    Local<String> ret;
    if (error->ToString(context).ToLocal(&ret)) {
      return ret;
    } else {
      return MaybeLocal<Value>();
    }
  }
  Local<Value> args[] = {
      context->Global(),
      error,
      sites,
  };
  // This TryCatch + Rethrow is required by V8 due to details around exception
  // handling there. For C++ callbacks, V8 expects a scheduled exception (which
  // is what ReThrow gives us). Just returning the empty MaybeLocal would leave
  // us with a pending exception.
  v8::TryCatch try_catch(isolate);
  MaybeLocal<Value> result = prepare.ToLocalChecked().As<Function>()->Call(
      context, Undefined(isolate), 3, args);
  if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
    try_catch.ReThrow();
  }
  return result;
}

void SetIsolateHandlers(Immortal* immortal, v8::Isolate* isolate) {
  isolate->SetHostImportModuleDynamicallyCallback(
      ModuleWrap::HostImportModuleDynamicallyWithImportAssertionsCallback);
  isolate->SetHostInitializeImportMetaObjectCallback(
      ModuleWrap::HostInitializeImportMetaObjectCallback);

  isolate->SetFatalErrorHandler(FatalError);
  isolate->SetPromiseRejectCallback(errors::OnPromiseReject);
  isolate->SetPrepareStackTraceCallback(PrepareStackTraceCallback);
  isolate->AddMessageListener(errors::OnMessage);
  v8::CpuProfiler::UseDetailedSourcePositionsForProfiling(isolate);
}

IsolateData::IsolateData(v8::Isolate* isolate,
                         const std::vector<size_t>* indexes)
    : isolate_(isolate) {
  if (indexes != nullptr) {
    DeserializeProperties(indexes);
  } else {
    CreateProperties();
  }
}

std::vector<size_t> IsolateData::Serialize(v8::SnapshotCreator* creator) {
  CHECK_EQ(isolate_, creator->GetIsolate());
  std::vector<size_t> indexes;
  HandleScope handle_scope(isolate_);
#define VP(name, value) V(Private, name##_symbol)
#define VS(name, value) V(String, name##_string)
#define V(TypeName, name)                                                      \
  indexes.push_back(creator->AddData(name##_.Get(isolate_)));
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VS
#undef VP

  return indexes;
}

void IsolateData::DeserializeProperties(const std::vector<size_t>* indexes) {
  size_t i = 0;
  HandleScope handle_scope(isolate_);

#define VP(name, value) V(Private, name##_symbol)
#define VS(name, value) V(String, name##_string)
#define V(TypeName, name)                                                      \
  do {                                                                         \
    MaybeLocal<TypeName> maybe_field =                                         \
        isolate_->GetDataFromSnapshotOnce<TypeName>((*indexes)[i++]);          \
    Local<TypeName> field;                                                     \
    if (!maybe_field.ToLocal(&field)) {                                        \
      fprintf(stderr, "Failed to deserialize " #name "\n");                    \
    }                                                                          \
    name##_.Reset(isolate_, field);                                            \
  } while (0);
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VS
#undef VP
}

void IsolateData::CreateProperties() {
  HandleScope handle_scope(isolate_);
#define V(name, value)                                                         \
  {                                                                            \
    MaybeLocal<String> maybe_string = String::NewFromUtf8(isolate_, value);    \
    CHECK(!maybe_string.IsEmpty());                                            \
    auto symbol = v8::Private::New(isolate_, maybe_string.ToLocalChecked());   \
    name##_symbol_.Reset(isolate_, symbol);                                    \
  }
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(V)
#undef V

#define V(name, value)                                                         \
  {                                                                            \
    MaybeLocal<String> maybe_string = String::NewFromUtf8(isolate_, value);    \
    CHECK(!maybe_string.IsEmpty());                                            \
    name##_string_.Reset(isolate_, maybe_string.ToLocalChecked());             \
  }
  PER_ISOLATE_STRING_PROPERTIES(V);
#undef V
}

Immortal::Immortal(uv_loop_t* loop,
                   CommandlineParserGroup* options,
                   v8::Isolate* isolate,
                   IsolateData* isolate_data,
                   const std::vector<size_t>* indexes)
    : isolate_(isolate),
      commandline_parser_(options),
      event_loop_(loop),
      env_vars_(std::make_shared<ProcessEnvStore>()),
      cache_storage_(nullptr),
      callback_scope_stack_top_(nullptr),
      macro_task_queue_(MacroTaskQueue::Create(
          loop, options->max_macro_task_count_per_tick())),
      isolate_data_(isolate_data) {
  interrupt_async_ = UvAsync<std::function<void()>>::Create(
      loop, [this]() { ExhaustInterruptRequests(); });
  interrupt_async_->Unref();

  CHECK_EQ(uv_prepare_init(loop, &prepare_handle_), 0);
  CHECK_EQ(uv_check_init(loop, &check_handle_), 0);
  uv_prepare_start(&prepare_handle_, OnPrepare);
  uv_check_start(&check_handle_, OnCheck);
  uv_unref(reinterpret_cast<uv_handle_t*>(&prepare_handle_));
  uv_unref(reinterpret_cast<uv_handle_t*>(&check_handle_));

  worker_state_ = WorkerState::kBootstrapping;

  // TODO(chengzhong.wcz): this really needs to be moved to cli options.
  if (options->mode() == "seed") {
    startup_fork_mode_ = StartupForkMode::kFork;
  } else if (options->mode() == "seed-userland") {
    startup_fork_mode_ = StartupForkMode::kForkUserland;
  } else {
    startup_fork_mode_ = StartupForkMode::kNone;
  }

  InitializeIsolate();

  if (indexes != nullptr) {
    Deserialize(indexes);
  } else {
    InitializeDefaultContext();
  }
}

Immortal::~Immortal() {
  uv_close(reinterpret_cast<uv_handle_t*>(&prepare_handle_), nullptr);
  uv_close(reinterpret_cast<uv_handle_t*>(&check_handle_), nullptr);
  StopAgent();
  for (HandleWrap* hw : tracked_handle_wraps) {
    hw->Close(Local<Value>());
  }
  uv_run(event_loop(), UV_RUN_ONCE);

  std::unordered_map<int, ModuleWrap*> map = hash_to_module_map;
  for (auto it : map) {
    delete it.second;
  }

  // TODO(chengzhong.wcz): distinguishes the native object allocated during
  // bootstrap (like globalThis.location) and the ones allocated by
  // user-scripts.
  // Release managed base objects.
#define V(type, name) name##_.Reset();
  IMMORTAL_GLOBAL_PROPERTIES(V)
  // We can't delete `it` here directly because `it`'s deconstructor erases
  // itself from `tracked_base_objects`. So we copy it and delete them.
  vector<BaseObject*> bo_array(tracked_base_objects.cbegin(),
                               tracked_base_objects.cend());

  for (size_t i = 0; i < bo_array.size(); i++) {
    delete bo_array[i];
  }
#undef V
}

void Immortal::BuildEmbedderGraph(v8::Isolate* isolate,
                                  v8::EmbedderGraph* graph,
                                  void* data) {
  Immortal* immortal = static_cast<Immortal*>(data);
  MemoryTracker tracker(isolate, graph);

  tracker.Track(immortal);
}

void Immortal::MemoryInfo(MemoryTracker* tracker) const {
  for (auto it : tracked_base_objects) {
    tracker->Track(it);
  }
}

std::vector<size_t> Immortal::Serialize(v8::SnapshotCreator* creator) {
  CHECK_EQ(isolate_, creator->GetIsolate());
  std::vector<size_t> indexes;
  HandleScope handle_scope(isolate_);
  Local<Context> context = context_.Get(isolate_);
#define V(TypeName, name)                                                      \
  if (name##_.IsEmpty()) {                                                     \
    indexes.push_back(0);                                                      \
  } else {                                                                     \
    indexes.push_back(creator->AddData(context, name##_.Get(isolate_)));       \
  }
  IMMORTAL_GLOBAL_PROPERTIES(V)
#undef V

  for (auto it : tracked_base_objects) {
    per_process::Debug(DebugCategory::MKSNAPSHOT,
                       "Serialize tracked object %s(%p)\n",
                       it->GetWrapperTypeInfo()->interface_name,
                       it);
    CHECK(it->is_snapshotable());
    indexes.push_back(creator->AddData(context, it->object()));
  }

  return indexes;
}

void Immortal::Deserialize(const std::vector<size_t>* indexes) {
  size_t i = 0;
  HandleScope handle_scope(isolate_);

  // TODO(chengzhong.wcz): ability to set index
  DeserializeContext des_context(this);
  Local<Context> context =
      Context::FromSnapshot(isolate(),
                            AworkerMainInstance::kMainContextIndex,
                            {DeserializeAworkerInternalFields, &des_context})
          .ToLocalChecked();
  AssignToContext(context);
  set_context(context);

#define V(TypeName, name)                                                      \
  do {                                                                         \
    size_t idx = (*indexes)[i++];                                              \
    if (idx == 0) break;                                                       \
    MaybeLocal<TypeName> maybe_field =                                         \
        context->GetDataFromSnapshotOnce<TypeName>(idx);                       \
    Local<TypeName> field;                                                     \
    if (!maybe_field.ToLocal(&field)) {                                        \
      fprintf(stderr, "Failed to deserialize " #name "\n");                    \
    }                                                                          \
    name##_.Reset(isolate_, field);                                            \
  } while (0);
  IMMORTAL_GLOBAL_PROPERTIES(V)
#undef V

  des_context.RunDeserializeRequests();
}

void Immortal::StopAgent() {
  if (inspector_agent_ != nullptr) {
    inspector_agent_->Stop();
    inspector_agent_.reset();
  }
  /**
   * Shutdown watchdog thread before the diag channel and watchdogs destruction.
   */
  watchdog_.reset();
  report_watchdog_.reset();
  loop_latency_watchdog_.reset();

  if (agent_data_channel_ != nullptr) {
    agent_data_channel_.reset();
  }
  if (agent_diag_channel_ != nullptr) {
    agent_diag_channel_.reset();
  }
}

v8::Maybe<bool> Immortal::RunCleanupHooks() {
  TryCatchScope try_catch(this, CatchMode::kFatal);
  HandleScope scope(isolate());
  Local<Context> context = this->context();
  Local<Object> process_object = this->process_object();
  Local<String> run_cleanup_hooks_string = this->run_cleanup_hooks_string();
  Local<Value> run_cleanup_hooks_function =
      process_object->Get(context, run_cleanup_hooks_string).ToLocalChecked();
  MaybeLocal<Value> ret = run_cleanup_hooks_function.As<Function>()->Call(
      context, process_object, 0, nullptr);
  if (ret.IsEmpty()) {
    DCHECK(try_catch.HasCaught());
    return v8::Nothing<bool>();
  }

  return v8::Just(true);
}

void Immortal::Exit(int code) {
  exit(code);
}

Local<String> CreateName(Isolate* isolate, const char* name) {
  MaybeLocal<String> maybe_name = String::NewFromUtf8(isolate, name);
  CHECK(!maybe_name.IsEmpty());

  Local<String> v8_name = maybe_name.ToLocalChecked();
  return v8_name;
}

bool Immortal::SetAccessor(Local<Object> object,
                           const char* name,
                           V8RawGetter getter,
                           V8RawSetter setter) {
  auto context_ = context();
  Local<String> v8_name = CreateName(isolate(), name);

  Maybe<bool> ret = object->SetAccessor(context_, v8_name, getter, setter);
  CHECK(ret.IsJust());

  return ret.FromJust();
}

bool Immortal::SetAccessor(Local<ObjectTemplate> tpl,
                           const char* name,
                           V8RawGetter getter,
                           V8RawSetter setter) {
  Local<String> v8_name = CreateName(isolate(), name);

  tpl->SetAccessor(v8_name, getter, setter);
  return true;
}

bool Immortal::SetFunctionProperty(Local<Object> object,
                                   const char* name,
                                   V8RawFunction func) {
  auto context_ = context();
  Local<FunctionTemplate> func_tpl = FunctionTemplate::New(isolate_, func);
  MaybeLocal<Function> maybe_func = func_tpl->GetFunction(context_);
  CHECK(!maybe_func.IsEmpty());

  Local<String> v8_name = CreateName(isolate(), name);
  Local<Function> v8_func = maybe_func.ToLocalChecked();

  v8_func->SetName(v8_name);
  Maybe<bool> ret = object->Set(context_, v8_name, v8_func);
  CHECK(ret.IsJust());

  return ret.FromJust();
}

bool Immortal::SetFunctionProperty(Local<ObjectTemplate> tpl,
                                   const char* name,
                                   V8RawFunction func) {
  Local<FunctionTemplate> func_tpl = FunctionTemplate::New(isolate_, func);

  Local<String> v8_name = CreateName(isolate(), name);

  func_tpl->SetClassName(v8_name);
  tpl->Set(v8_name, func_tpl);

  return true;
}

bool Immortal::SetIntegerProperty(Local<Object> object,
                                  const char* name,
                                  int num) {
  auto context_ = context();
  Local<String> v8_name = CreateName(isolate(), name);

  Maybe<bool> ret =
      object->Set(context_, v8_name, v8::Int32::New(isolate_, num));
  CHECK(ret.IsJust());

  return ret.FromJust();
}

bool Immortal::SetValueProperty(Local<Object> object,
                                const char* name,
                                Local<Value> value) {
  auto context_ = context();
  Local<String> v8_name = CreateName(isolate(), name);

  Maybe<bool> ret = object->Set(context_, v8_name, value);
  CHECK(ret.IsJust());

  return ret.FromJust();
}

void Immortal::RequestInterrupt(std::function<void()>&& callback) {
  {
    ScopedLock lock(interrupt_mutex_);
    interrupt_requests_.push_back(callback);
  }

  interrupt_async_->Send();
  isolate()->RequestInterrupt(
      [](v8::Isolate* isolate, void* ctx) {
        auto immortal = reinterpret_cast<Immortal*>(ctx);
        immortal->ExhaustInterruptRequests();
      },
      this);
}

void Immortal::ExhaustInterruptRequests() {
  std::list<std::function<void()>> requests;
  {
    ScopedLock lock(interrupt_mutex_);
    requests.swap(interrupt_requests_);
  }
  for (auto it : requests) {
    it();
  }
}

void Immortal::InitializeIsolate() {
  isolate_->SetMicrotasksPolicy(v8::MicrotasksPolicy::kExplicit);
  isolate_->GetHeapProfiler()->AddBuildEmbedderGraphCallback(BuildEmbedderGraph,
                                                             this);
}

void Immortal::InitializeDefaultContext() {
  DCHECK_NE(isolate(), nullptr);
  HandleScope scope(isolate());

  Isolate::Scope isolate_scope(isolate());
  HandleScope handle_scope(isolate());

  Local<Context> context = Context::New(isolate());
  AssignToContext(context);
  set_context(context);
}

bool Immortal::Bootstrap() {
  HandleScope scope(isolate());
  Context::Scope context_scope(context());
  if (!BootstrapLoader()) {
    return false;
  }
  return BootstrapPureNativeModules();
}

bool Immortal::BootstrapLoader() {
  HandleScope scope(isolate());
  Local<Context> ctx = context();

  Local<Function> get_native_module =
      FunctionTemplate::New(isolate(),
                            NativeModuleManager::GetNativeModuleFunction)
          ->GetFunction(ctx)
          .ToLocalChecked();

  Local<Function> get_binding =
      FunctionTemplate::New(isolate(), binding::GetBinding)
          ->GetFunction(ctx)
          .ToLocalChecked();

  TryCatchScope try_catch_scope(this, CatchMode::kFatal);

  NativeModuleResult result;
  Local<Function> func =
      NativeModuleManager::Compile(this, "bootstrap/loader.js", &result)
          .ToLocalChecked();
  DCHECK_EQ(result.type, NativeModuleType::Loader);

  Local<Value> args[] = {get_native_module, get_binding};
  Local<Value> loader_exports =
      func->Call(ctx, Undefined(isolate()), arraysize(args), args)
          .ToLocalChecked();
  CHECK(loader_exports->IsObject());
  Local<Object> loader_exports_obj = loader_exports.As<Object>();

  Local<Value> load_function =
      loader_exports_obj->Get(ctx, OneByteString(isolate(), "load"))
          .ToLocalChecked();
  CHECK(load_function->IsFunction());
  set_loader_function(load_function.As<Function>());

  Local<Value> load_binding_function =
      loader_exports_obj->Get(ctx, OneByteString(isolate(), "loadBinding"))
          .ToLocalChecked();
  CHECK(load_binding_function->IsFunction());
  set_load_binding_function(load_binding_function.As<Function>());

  return true;
}

bool Immortal::BootstrapPureNativeModules() {
  Isolate::Scope isolate_scope(isolate());
  HandleScope scope(isolate());

  MaybeLocal<Value> maybe_value;
  {
    TryCatchScope try_catch_scope(this, CatchMode::kFatal);

    NativeModuleResult result;
    Local<Function> func =
        NativeModuleManager::Compile(this, "bootstrap/aworker.js", &result)
            .ToLocalChecked();
    DCHECK_EQ(result.type, NativeModuleType::Bootstrapping);

    Local<Value> args[] = {loader_function(), load_binding_function()};
    maybe_value =
        func->Call(context(), Undefined(isolate()), arraysize(args), args);
  }
  CHECK(!maybe_value.IsEmpty());

  return true;
}

void Immortal::BootstrapPerExecution() {
  HandleScope scope(isolate());
  Context::Scope context_scope(context());
  inspector_agent_ = std::make_shared<inspector::InspectorAgent>(this);
  BootstrapContextPerExecution();
}

void Immortal::BootstrapAgent(const std::string& script_filename) {
  TRACE_EVENT0(TRACING_CATEGORY_AWORKER1(main), "BootstrapAgent");

  watchdog_ = std::make_unique<Watchdog>();
  {
    HandleScope scope(isolate());
    CHECK(!agent_channel_callback().IsEmpty());
    CHECK(!agent_channel_handler().IsEmpty());
  }

  if (commandline_parser()->mixin_has_agent()) {
    StartAgentChannel();
    agent_data_channel()->WaitForAgent();
    CHECK(agent_data_channel()->connected());
  }

  if (commandline_parser()->mixin_inspect()) {
    inspector_agent_->Start(script_filename);
  }

  /**
   * Start watchdog thread.
   */
  if (commandline_parser()->report_on_signal()) {
    report_watchdog_ = std::make_unique<report::ReportWatchdog>(this);
  }
  watchdog_->StartIfNeeded();

  if (commandline_parser()->inspect_brk()) {
    CHECK(inspector_agent_ != nullptr);
    inspector_agent_->WaitForConnect();
  }
}

void Immortal::StartLoopLatencyWatchdog() {
  if (loop_latency_watchdog_) {
    return;
  }
  DCHECK(commandline_parser()->loop_latency_limit_ms());
  loop_latency_watchdog_ = std::make_unique<LoopLatencyWatchdog>(
      this, commandline_parser()->loop_latency_limit_ms());
  watchdog_->StartIfNeeded();
  // Initiate initial loop check.
  loop_latency_watchdog_->OnCheck();
}

void Immortal::StartAgentChannel() {
  auto parser = commandline_parser();
  if (!parser->mixin_has_agent()) return;

  if (parser->agent_ipc_path() == "") {
    fprintf(stderr,
            "FATAL: (start) Agent IPC path unspecified under agent "
            "mode.\n");
    exit(4);
  }

  std::shared_ptr<AgentDataChannel> data_channel =
      std::make_shared<agent::NoslatedDataChannel>(this,
                                                   parser->agent_ipc_path(),
                                                   parser->agent_cred(),
                                                   parser->ref_agent());
  std::shared_ptr<AgentDiagChannel> diag_channel =
      std::make_shared<agent::NoslatedDiagChannel>(
          this, parser->agent_ipc_path(), parser->agent_cred());

  set_agent_data_channel(data_channel);
  set_agent_diag_channel(diag_channel);
}

void Immortal::BootstrapContextPerExecution() {
  TRACE_EVENT0(TRACING_CATEGORY_AWORKER1(main), "BootstrapContextPerExecution");
  /**
   * Isolate handlers must be set after recovering from snapshots.
   */
  SetIsolateHandlers(this, isolate_);

  Isolate::Scope isolate_scope(isolate());
  HandleScope scope(isolate());

  MaybeLocal<Value> maybe_value;
  {
    TryCatchScope try_catch_scope(this, CatchMode::kFatal);

    NativeModuleResult result;
    Local<Function> func = NativeModuleManager::Compile(
                               this, "bootstrap/per_execution.js", &result)
                               .ToLocalChecked();
    DCHECK_EQ(result.type, NativeModuleType::Bootstrapping);

    Local<Value> args[] = {loader_function(), load_binding_function()};
    maybe_value =
        func->Call(context(), Undefined(isolate()), arraysize(args), args);
  }
  CHECK(!maybe_value.IsEmpty());
}

v8::Maybe<bool> Immortal::Evaluate(const char* module) {
  HandleScope scope(isolate());
  Context::Scope context_scope(context());

  NativeModuleResult result;
  Local<Function> func =
      NativeModuleManager::Compile(this, module, &result).ToLocalChecked();
  DCHECK_EQ(result.type, NativeModuleType::Bootstrapping);

  Local<Value> args[] = {loader_function(), load_binding_function()};
  MaybeLocal<Value> ret =
      func->Call(context(), Undefined(isolate()), arraysize(args), args);
  if (ret.IsEmpty()) {
    return v8::Nothing<bool>();
  }

  task_queue::TickTaskQueue(this);

  return v8::Just(true);
}

v8::Maybe<bool> Immortal::StartForkExecution() {
  per_process::DebugPrintCurrentTime("execute fork main");
  TRACE_EVENT0(TRACING_CATEGORY_AWORKER1(main), "ForkFirstFrame");

  return Evaluate("bootstrap/main_fork.js");
}

v8::Maybe<bool> Immortal::StartExecution() {
  per_process::DebugPrintCurrentTime("execute main");
  TRACE_EVENT0(TRACING_CATEGORY_AWORKER1(main), "FirstFrame");

  return Evaluate("bootstrap/main.js");
}

// static
void Immortal::OnPrepare(uv_prepare_t* handle) {
  Immortal* immortal = ContainerOf(&Immortal::prepare_handle_, handle);
  immortal->isolate()->SetIdle(true);
  if (immortal->loop_latency_watchdog_) {
    immortal->loop_latency_watchdog_->OnPrepare();
  }
}

// static
void Immortal::OnCheck(uv_check_t* handle) {
  Immortal* immortal = ContainerOf(&Immortal::check_handle_, handle);
  immortal->isolate()->SetIdle(false);
  if (immortal->loop_latency_watchdog_) {
    immortal->loop_latency_watchdog_->OnCheck();
  }
}

}  // namespace aworker
