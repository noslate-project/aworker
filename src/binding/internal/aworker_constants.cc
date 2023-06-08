#include <cmath>

#include "aworker_binding.h"
#include "aworker_perf.h"
#include "immortal.h"
#include "tracing/trace_event.h"
#include "util.h"

namespace aworker {
namespace constants {

using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;

#define DEFINE_CONSTANT(target, constant)                                      \
  DEFINE_CONSTANT_T(target, constant, #constant)

#define DEFINE_CONSTANT_NAME(target, constant, name)                           \
  DEFINE_CONSTANT_T(target, constant, #name)

#define DEFINE_CONSTANT_T(target, constant, name)                              \
  do {                                                                         \
    v8::Isolate* isolate = target->GetIsolate();                               \
    v8::Local<v8::Context> context = isolate->GetCurrentContext();             \
    v8::Local<v8::String> constant_name = OneByteString(isolate, name);        \
    v8::Local<v8::Number> constant_value =                                     \
        v8::Number::New(isolate, static_cast<double>(constant));               \
    v8::PropertyAttribute constant_attributes =                                \
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete);     \
    (target)                                                                   \
        ->DefineOwnProperty(                                                   \
            context, constant_name, constant_value, constant_attributes)       \
        .Check();                                                              \
  } while (0)

void DefineTraceConstants(Local<Object> target) {
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_BEGIN);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_END);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_COMPLETE);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_INSTANT);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_ASYNC_BEGIN);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_ASYNC_STEP_INTO);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_ASYNC_STEP_PAST);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_ASYNC_END);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_NESTABLE_ASYNC_END);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_FLOW_BEGIN);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_FLOW_STEP);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_FLOW_END);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_METADATA);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_COUNTER);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_SAMPLE);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_CREATE_OBJECT);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_SNAPSHOT_OBJECT);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_DELETE_OBJECT);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_MEMORY_DUMP);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_MARK);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_CLOCK_SYNC);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_ENTER_CONTEXT);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_LEAVE_CONTEXT);
  DEFINE_CONSTANT(target, TRACE_EVENT_PHASE_LINK_IDS);
}

void DefineSignalConstants(Local<Object> target) {
  DEFINE_CONSTANT(target, SIGTERM);
  DEFINE_CONSTANT(target, SIGINT);
}

void DefineMilestonesConstants(Local<Object> target) {
#define V(name, _)                                                             \
  DEFINE_CONSTANT(target, AWORKER_PERFORMANCE_MILESTONE_##name);
  AWORKER_PERFORMANCE_MILESTONES(V)
#undef V
}

Local<Object> DefinePrivateSymbols(Immortal* immortal) {
  Local<ObjectTemplate> tmpl = ObjectTemplate::New(immortal->isolate());
#define V(PropertyName, _)                                                     \
  tmpl->Set(OneByteString(immortal->isolate(), #PropertyName),                 \
            immortal->PropertyName##_symbol());

  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(V)
#undef V

  return tmpl->NewInstance(immortal->context()).ToLocalChecked();
}

AWORKER_BINDING(Init) {
  Isolate* isolate = immortal->isolate();
  Local<Object> tracing_constants = Object::New(isolate);
  CHECK(tracing_constants->SetPrototype(context, v8::Null(isolate)).FromJust());
  DefineTraceConstants(tracing_constants);

  Local<Object> signals_constants = Object::New(isolate);
  CHECK(signals_constants->SetPrototype(context, v8::Null(isolate)).FromJust());
  DefineSignalConstants(signals_constants);

  Local<Object> perf_milestones = Object::New(isolate);
  CHECK(perf_milestones->SetPrototype(context, v8::Null(isolate)).FromJust());
  DefineMilestonesConstants(perf_milestones);

  Local<Object> private_symbols = DefinePrivateSymbols(immortal);
  CHECK(private_symbols->SetPrototype(context, v8::Null(isolate)).FromJust());

  DEFINE_CONSTANT_NAME(
      exports, WorkerState::kSerialized, WORKER_STATE_SERIALIZED);
  DEFINE_CONSTANT_NAME(
      exports, WorkerState::kDeserialized, WORKER_STATE_DESERIALIZED);
  DEFINE_CONSTANT_NAME(
      exports, WorkerState::kForkPrepared, WORKER_STATE_FORK_PREPARED);
  DEFINE_CONSTANT_NAME(
      exports, WorkerState::kInstalled, WORKER_STATE_INSTALLED);
  DEFINE_CONSTANT_NAME(
      exports, WorkerState::kActivated, WORKER_STATE_ACTIVATED);

  DEFINE_CONSTANT_NAME(exports, StartupForkMode::kNone, STARTUP_FORK_MODE_NONE);
  DEFINE_CONSTANT_NAME(exports, StartupForkMode::kFork, STARTUP_FORK_MODE_FORK);
  DEFINE_CONSTANT_NAME(
      exports, StartupForkMode::kForkUserland, STARTUP_FORK_MODE_FORK_USERLAND);

  exports->Set(context, OneByteString(isolate, "tracing"), tracing_constants)
      .Check();

  exports->Set(context, OneByteString(isolate, "signals"), signals_constants)
      .Check();

  exports->Set(context, OneByteString(isolate, "milestones"), perf_milestones)
      .Check();

  exports
      ->Set(context, OneByteString(isolate, "private_symbols"), private_symbols)
      .Check();
}

AWORKER_EXTERNAL_REFERENCE(Init) {}

}  // namespace constants
}  // namespace aworker

AWORKER_BINDING_REGISTER(constants,
                         aworker::constants::Init,
                         aworker::constants::Init)
