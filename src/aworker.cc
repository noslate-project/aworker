#include "aworker.h"

extern "C" {
#include <turf_phd.h>
}
#include <stdio.h>
#include <unistd.h>
#include <memory>
#include <string>

#include "libplatform/libplatform.h"
#include "v8.h"

#include "agent_channel/alice_data_channel.h"
#include "aworker_binding.h"
#include "aworker_logger.h"
#include "aworker_perf.h"
#include "aworker_platform.h"
#include "aworker_v8.h"
#include "aworker_version.h"
#include "command_parser.h"
#include "debug_utils.h"
#include "immortal.h"
#include "inspector/inspector_agent.h"
#include "native_module_manager.h"
#include "snapshot/embedded_snapshot_data.h"
#include "snapshot/snapshot_builder.h"
#include "task_queue.h"
#include "tracing/trace_event.h"
#include "util.h"

#include "external_references.h"

using std::string;
using v8::Array;
using v8::ArrayBuffer;
using v8::Context;
using v8::Exception;
using v8::FunctionTemplate;
using v8::Global;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Message;
using v8::ObjectTemplate;
using v8::Platform;
using v8::SealHandleScope;
using v8::TryCatch;
using v8::Undefined;
using v8::V8;
using v8::Value;

namespace aworker {
namespace per_process {
bool external_snapshot_loaded = false;
}  // namespace per_process

/**
 * Disable all known concurrent/parallel tasks to ensure all tasks are run on
 * JavaScript thread and no worker thread tasks.
 */
constexpr inline const char* v8_flags() {
  return "--single-threaded";
}

int64_t GenerateSeed() {
  char buf[8];
  memset(buf, 0, sizeof(buf));
  CHECK_EQ(uv_random(NULL, NULL, buf, sizeof(buf), 0, NULL), 0);
  return *reinterpret_cast<int64_t*>(buf);
}

// TODO(chengzhong.wcz): parse per_process options
char** InitializeOncePerProcess(int* argc, char** argv) {
  per_process::enabled_debug_list.Parse();
  RefreshLogLevel();
  per_process::DebugPrintCurrentTime("initialize once per process");

  RegisterBindings();
  per_process::external_reference_registry.Collect();

  argv = uv_setup_args(*argc, argv);

  // check `--help` to avoid passing it into V8
  int help_count = 0;
  char* help_ptr = nullptr;
  for (int i = 0; i < *argc; i++) {
    if (strcmp(argv[i], "--help") == 0) {
      help_count++;
      help_ptr = argv[i];
    } else {
      argv[i - help_count] = argv[i];
    }
  }
  *argc -= help_count;

  /**
   * V8::SetFlagsFromCommandLine doesn't enforce flag implications.
   * We need to use V8::SetFlagsFromString to set internal flags and their
   * implications.
   */
  V8::SetFlagsFromCommandLine(argc, argv, true);
  /**
   * Disable all known concurrent/parallel tasks to ensure all tasks are run on
   * JavaScript thread and no worker thread tasks.
   */
  V8::SetFlagsFromString(v8_flags());

  if (help_count) {
    argv[*argc] = help_ptr;
    (*argc)++;
  }

  return argv;
}

bool TearDownOncePerProcess() {
  return true;
}

void AworkerMainInstance::DisposeIsolate(v8::Isolate* isolate) {
  isolate->Dispose();
}

AworkerMainInstance::AworkerMainInstance(
    AworkerPlatform* platform, std::unique_ptr<CommandlineParserGroup> cli)
    : cli_(std::move(cli)), platform_(platform), loop_(platform->loop()) {
  isolate_owner_ = IsolatePtr(v8::Isolate::Allocate());
  isolate_ = isolate_owner_.get();
  isolate_->Enter();
}

AworkerMainInstance::~AworkerMainInstance() {
  // Release Immortal first then teardown the per process resources.
  // TODO(chengzhong.wcz): move those loop dependant items to local scopes.
  // Loop handles should be closed and cleared first before teardown (in case
  // of close callbacks).
  // TODO(chengzhong.wcz): find a way to automatically reset in order.
  immortal_owner_.reset();
  if (isolate_owner_ != nullptr) {
    isolate_->Exit();
  }
  isolate_data_owner_.reset();
  isolate_owner_.reset();
  cli_.reset();
}

v8::StartupData* AworkerMainInstance::GetSnapshotBlob() {
  if (cli_->build_snapshot()) {
    return nullptr;
  }
  if (cli_->has_snapshot_blob() && per_process::external_snapshot_loaded) {
    CHECK_NE(snapshot_data_, nullptr);
    return &snapshot_data_->blob;
  }
  return EmbeddedSnapshotData::GetSnapshotBlob();
}

const std::vector<size_t>* AworkerMainInstance::GetIsolateDataIndexes() {
  if (cli_->build_snapshot()) {
    return nullptr;
  }
  if (cli_->has_snapshot_blob() && per_process::external_snapshot_loaded) {
    CHECK_NE(snapshot_data_, nullptr);
    return &snapshot_data_->isolate_data_indexes;
  }
  return EmbeddedSnapshotData::GetIsolateDataIndexes();
}

const std::vector<size_t>* AworkerMainInstance::GetImmortalDataIndexes() {
  if (cli_->build_snapshot()) {
    return nullptr;
  }
  if (cli_->has_snapshot_blob() && per_process::external_snapshot_loaded) {
    CHECK_NE(snapshot_data_, nullptr);
    return &snapshot_data_->immortal_data_indexes;
  }
  return EmbeddedSnapshotData::GetImmortalDataIndexes();
}

const std::vector<NativeModuleCache>*
AworkerMainInstance::GetNativeModuleCaches() {
  if (cli_->build_snapshot()) {
    return nullptr;
  }
  if (cli_->has_snapshot_blob() && per_process::external_snapshot_loaded) {
    CHECK_NE(snapshot_data_, nullptr);
    return &snapshot_data_->native_module_caches;
  }
  return EmbeddedSnapshotData::GetNativeModuleCaches();
}

void AworkerMainInstance::WarmFork() {
  Isolate::DisallowJavascriptExecutionScope disallow_js(
      isolate_, Isolate::DisallowJavascriptExecutionScope::CRASH_ON_FAILURE);
  TraceAgent::SuspendTracingScope suspend_tracing(platform_->trace_agent());

  int argc = 1;
  std::string kWarmForkExecArgv = "aworker";
  std::vector<char*> argv_vec{const_cast<char*>(kWarmForkExecArgv.data())};
  char** argv = argv_vec.data();
  int rc = 0;

  TURF_PHD(turf_fork_wait, &rc, &argc, &argv);
  RefreshTimeOrigin();
  RefreshLogLevel();
  per_process::enabled_debug_list.Parse();
  per_process::DebugPrintCurrentTime("just after fork");
  if (rc == -1) {
    fprintf(stderr,
            "Not run in a warm-forkable environment. You may be running in a "
            "testing environment.\n");
  }

  uv_loop_fork(loop_);
  per_process::DebugPrintCurrentTime("uv after fork");

  // Pass argv to V8 and left argv filtered to commandline parser
  V8::SetFlagsFromCommandLine(&argc, argv, true);

  cli_->PushCommandlineParser("fork", argc, argv);
  cli_->EvaluateLast();

  HandleScope scope(isolate_);
  Local<Context> context = immortal_->context();
  aworker_v8::ReconfigureHeapAfterResetV8Options(isolate_);
  aworker_v8::ResetRandomNumberGenerator(isolate_, context, GenerateSeed());

  platform_->EvaluateCommandlineOptions(cli_.get());
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0(TRACING_CATEGORY_AWORKER1(main),
                                      "warmFork",
                                      TRACE_EVENT_SCOPE_THREAD,
                                      uv_hrtime() / 1000);
}

void AworkerMainInstance::Initialize(IsolateCreationMode mode) {
  platform_->EvaluateCommandlineOptions(cli_.get());
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0(TRACING_CATEGORY_AWORKER1(main),
                                      "mainInstance",
                                      TRACE_EVENT_SCOPE_THREAD,
                                      uv_hrtime() / 1000);
  TRACE_EVENT0(TRACING_CATEGORY_AWORKER1(main), "Initialize");

  if (cli_->build_snapshot()) {
    CHECK_EQ(mode, IsolateCreationMode::kCreateSnapshot);
  } else if (cli_->has_snapshot_blob()) {
    TRACE_EVENT0(TRACING_CATEGORY_AWORKER1(main), "ReadSnapshotData");
    snapshot_data_ = SnapshotData::ReadFromFile(cli_->snapshot_blob());
    per_process::external_snapshot_loaded = snapshot_data_ != nullptr;
  }

  if (mode == IsolateCreationMode::kNormal) {
    v8::Isolate::CreateParams params;
    params.array_buffer_allocator_shared = platform_->array_buffer_allocator();
    params.snapshot_blob = GetSnapshotBlob();
    params.external_references =
        per_process::external_reference_registry.external_references().data();
    v8::Isolate::Initialize(isolate_, params);
  } else if (mode == IsolateCreationMode::kTesting) {
    v8::Isolate::CreateParams params;
    params.array_buffer_allocator_shared = platform_->array_buffer_allocator();
    params.external_references =
        per_process::external_reference_registry.external_references().data();
    v8::Isolate::Initialize(isolate_, params);
  }
  // Don't call v8::Isolate::Initialize on kCreateSnapshot.
  // In mksnapshot this is done by SnapshotCreator.

  isolate_data_owner_ = std::make_unique<IsolateData>(
      isolate_,
      mode == IsolateCreationMode::kTesting ? nullptr
                                            : GetIsolateDataIndexes());
  isolate_data_ = isolate_data_owner_.get();

  const std::vector<NativeModuleCache>* native_module_caches =
      GetNativeModuleCaches();
  if (native_module_caches) {
    NativeModuleManager::Instance().InitializeCachedData(*native_module_caches);
  }
  immortal_owner_ = std::make_unique<Immortal>(
      loop_,
      cli_.get(),
      isolate_,
      isolate_data_,
      mode == IsolateCreationMode::kTesting ? nullptr
                                            : GetImmortalDataIndexes());
  immortal_ = immortal_owner_.get();
}

void AworkerMainInstance::WarmForkWithUserScript() {
  Isolate* isolate = immortal_->isolate();
  std::string script_filename = cli_->script_filename();

  immortal_->BootstrapAgent(script_filename);
  per_process::DebugPrintCurrentTime("bootstrapped agent");

  CHECK_EQ(immortal_->StartForkExecution().ToChecked(), true);
  {
    // Disallow handles been created without local HandleScopes.
    SealHandleScope scope(isolate);
    while (uv_loop_alive(loop_)) {
      uv_run(loop_, UV_RUN_ONCE);
      if (immortal_->worker_state() == WorkerState::kForkPrepared) {
        break;
      }
    }
  }

  immortal_->StopAgent();
  WarmFork();
}

int AworkerMainInstance::Start() {
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0(TRACING_CATEGORY_AWORKER1(main),
                                      "loopStart",
                                      TRACE_EVENT_SCOPE_THREAD,
                                      uv_hrtime() / 1000);
  Isolate* isolate = immortal_->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal_->context();
  Context::Scope context_scope(context);

  if (GetSnapshotBlob() == nullptr && !immortal_->Bootstrap()) {
    fprintf(stderr, "Failed to bootstrap\n");
    exit(4);
  }

  {
    HandleScope scope(isolate);

    immortal_->BootstrapPerExecution();

    if (immortal_->startup_fork_mode() == StartupForkMode::kForkUserland) {
      WarmForkWithUserScript();
    } else if (immortal_->startup_fork_mode() == StartupForkMode::kFork) {
      WarmFork();
    }
    std::string script_filename = cli_->script_filename();

    immortal_->BootstrapAgent(script_filename);
    per_process::DebugPrintCurrentTime("bootstrapped agent");

    CHECK_EQ(immortal_->StartExecution().ToChecked(), true);
  }

  // 事件循环
  {
    // Disallow handles been created without local HandleScopes.
    SealHandleScope scope(isolate);
    uv_run(loop_, UV_RUN_DEFAULT);
  }

  if (cli_->mixin_inspect()) {
    immortal_->inspector_agent()->WaitForDisconnect();
  }

  immortal_->RunCleanupHooks();

  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0(TRACING_CATEGORY_AWORKER1(main),
                                      "loopExit",
                                      TRACE_EVENT_SCOPE_THREAD,
                                      uv_hrtime() / 1000);
  return 0;
}

int AworkerMainInstance::BuildSnapshot() {
  CHECK(cli_->has_snapshot_blob());
  aworker::SnapshotBuilder builder;
  aworker::SnapshotData data;
  builder.Generate(&data, platform_, this);
  data.WriteToFile(cli_->snapshot_blob());
  return 0;
}

}  // namespace aworker
