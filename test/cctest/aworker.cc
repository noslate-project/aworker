#include "aworker.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <iostream>
#include <set>

#include "aworker_binding.h"
#include "immortal.h"
#include "test_env.h"

namespace aworker {

using std::set;
using std::string;

void CheckBootstrapModules(Immortal* immortal,
                           set<string> expected_loaded_bindings,
                           set<string> expected_loaded_native_modules) {
  // Check bindings
  std::set<std::string> loaded_internal_binding_names;
  for (auto it : immortal->loaded_internal_bindings) {
    loaded_internal_binding_names.insert(std::string(it->name));
  }

  std::set<std::string> loaded_internal_bindings_diff;
  std::set_symmetric_difference(
      loaded_internal_binding_names.cbegin(),
      loaded_internal_binding_names.cend(),
      expected_loaded_bindings.cbegin(),
      expected_loaded_bindings.cend(),
      std::inserter(loaded_internal_bindings_diff,
                    loaded_internal_bindings_diff.begin()));
  EXPECT_EQ(loaded_internal_bindings_diff.size(), static_cast<size_t>(0));
  for (auto it : loaded_internal_bindings_diff) {
    if (expected_loaded_bindings.find(it) == expected_loaded_bindings.end()) {
      EXPECT_EQ(it, "THIS BINDING SHOULD NOT BE LOADED AT BOOTSTRAP");
    } else {
      EXPECT_EQ(it, "THIS BINDING SHOULD BE LOADED AT BOOTSTRAP");
    }
  }

  // Check native modules
  std::set<std::string> loaded_native_modules =
      immortal->loaded_native_modules_with_cache;
  for (auto it : immortal->loaded_native_modules_without_cache) {
    loaded_native_modules.insert(it);
  }

  std::set<std::string> loaded_native_modules_diff;
  std::set_symmetric_difference(
      loaded_native_modules.cbegin(),
      loaded_native_modules.cend(),
      expected_loaded_native_modules.cbegin(),
      expected_loaded_native_modules.cend(),
      std::inserter(loaded_native_modules_diff,
                    loaded_native_modules_diff.begin()));
  EXPECT_EQ(loaded_native_modules_diff.size(), static_cast<size_t>(0));
  for (auto it : loaded_native_modules_diff) {
    if (expected_loaded_native_modules.find(it) ==
        expected_loaded_native_modules.end()) {
      EXPECT_EQ(it, "THIS NATIVE MODULE SHOULD NOT BE LOADED AT BOOTSTRAP");
    } else {
      EXPECT_EQ(it, "THIS NATIVE MODULE SHOULD BE LOADED AT BOOTSTRAP");
    }
  }
}

TEST(Aworker, InitializeAndBootstrap) {
  char* argv[] = {"aworker"};  // NOLINT
  aworker::AworkerMainInstance instance(
      AworkerEnvironment::env()->platform(),
      std::make_unique<aworker::CommandlineParserGroup>(arraysize(argv), argv));
  instance.Initialize(IsolateCreationMode::kTesting);

  EXPECT_EQ(instance.immortal()->Bootstrap(), true);

  // Per-execution bindings like commandline options should not present in this
  // list.
  std::set<std::string> expected_loaded_bindings = {
      "noslated_data_channel",
      "bytes",
      "cache",
      "constants",
      "errors",
      "i18n",
      "perf",
      "process",
      "serdes",
      "aworker_options",
      "task_queue",
      "timers",
      "tracing",
      "types",
      "url",
  };

  // Per-execution modules like commandline options should not present in this
  // list.
  std::set<std::string> expected_loaded_native_modules = {
      "agent_channel.js",
      "assert.js",
      "bootstrap/loader.js",
      "bootstrap/aworker.js",
      "bytes.js",
      "cache.js",
      "console.js",
      "console/console.js",
      "console/debuglog.js",
      "console/format.js",
      "dom/abort_controller.js",
      "dom/abort_signal.js",
      "dom/event.js",
      "dom/event_objects.js",
      "dom/event_target.js",
      "dom/event_wrapper.js",
      "dom/exception.js",
      "dom/file.js",
      "dom/iterable.js",
      "dom/location.js",
      "encoding.js",
      "fetch/constants.js",
      "fetch/fetch.js",
      "fetch/body.js",
      "fetch/body_helper.js",
      "fetch/form.js",
      "fetch/headers.js",
      "global/aworker_namespace.js",
      "global/index.js",
      "global/service_worker_global_scope.js",
      "global/utils.js",
      "global/worker_global_scope.js",
      "navigator.js",
      "path.js",
      "performance/observer.js",
      "performance/performance.js",
      "performance/performance_entry.js",
      "performance/user_timing.js",
      "performance/utils.js",
      "process/execution.js",
      "service_worker/event.js",
      "service_worker/index.js",
      "source_maps/prepare_stack_trace_plain.js",
      "streams.js",
      "task_queue.js",
      "timer.js",
      "types.js",
      "url.js",
      "url/url.js",
      "utils.js",
  };

  CheckBootstrapModules(instance.immortal(),
                        expected_loaded_bindings,
                        expected_loaded_native_modules);
}

TEST(Aworker, InitializeAndBootstrapWithSnapshot) {
  char* argv[] = {"aworker"};  // NOLINT
  aworker::AworkerMainInstance instance(
      AworkerEnvironment::env()->platform(),
      std::make_unique<aworker::CommandlineParserGroup>(arraysize(argv), argv));
  instance.Initialize();

  // Per-execution bindings like commandline options should not present in this
  // list.
  std::set<std::string> expected_loaded_bindings = {};

  // Per-execution modules like commandline options should not present in this
  // list.
  std::set<std::string> expected_loaded_native_modules = {};

  CheckBootstrapModules(instance.immortal(),
                        expected_loaded_bindings,
                        expected_loaded_native_modules);
}
}  // namespace aworker
