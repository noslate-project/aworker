#include "task_queue.h"
#include "aworker_binding.h"
#include "error_handling.h"
#include "immortal.h"
#include "tracing/trace_event.h"
#include "v8.h"

namespace aworker {
namespace task_queue {

using v8::Context;
using v8::Function;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Value;

void TickTaskQueue(Immortal* immortal) {
  TRACE_EVENT0(TRACING_CATEGORY_AWORKER1(task_queue), "TickTaskQueue");
  // FIXME: should be replaced with asyncwrap to tick
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  {
    TryCatchScope try_catch_scope(immortal, CatchMode::kFatal);

    Local<Context> context = immortal->context();
    Local<Object> process_object = immortal->process_object();
    Local<String> tick_task_queue_string = immortal->tick_task_queue_string();
    // TODO(chengzhong.wcz): prevent from using property getters?
    Local<Value> tick_task_queue_function =
        process_object->Get(context, tick_task_queue_string).ToLocalChecked();
    if (!tick_task_queue_function->IsFunction()) {
      fprintf(stderr, "process.tickTaskQueue is not a function.");
      immortal->Exit(6);
      return;
    }

    MaybeLocal<Value> handled;
    Local<Value> argv[0] = {};
    handled = tick_task_queue_function.As<Function>()->Call(
        context, process_object, 0, argv);
    if (handled.IsEmpty()) {
      return;
    }

    if (handled.ToLocalChecked()->IsTrue()) {
      return;
    }
  }

  TickTaskQueue(immortal);
}

AWORKER_METHOD(EnqueueMicrotask) {
  Immortal* immortal = Immortal::GetCurrent(info);
  auto isolate = immortal->isolate();
  isolate->EnqueueMicrotask(info[0].As<Function>());
}

AWORKER_METHOD(PerformMicrotaskCheckpoint) {
  Immortal* immortal = Immortal::GetCurrent(info);
  auto isolate = immortal->isolate();
  isolate->PerformMicrotaskCheckpoint();
}

AWORKER_BINDING(Init) {
  immortal->SetFunctionProperty(exports, "enqueueMicrotask", EnqueueMicrotask);
  immortal->SetFunctionProperty(
      exports, "performMicrotaskCheckpoint", PerformMicrotaskCheckpoint);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  registry->Register(EnqueueMicrotask);
  registry->Register(PerformMicrotaskCheckpoint);
}

}  // namespace task_queue
}  // namespace aworker

AWORKER_BINDING_REGISTER(task_queue,
                         aworker::task_queue::Init,
                         aworker::task_queue::Init)
