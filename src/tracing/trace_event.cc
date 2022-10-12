#include "tracing/trace_event.h"
#include "tracing/trace_agent.h"

namespace aworker {
namespace tracing {

TraceAgent* g_agent = nullptr;
v8::TracingController* g_controller = nullptr;

void TraceEventHelper::SetTraceAgent(TraceAgent* agent) {
  g_agent = agent;
  if (agent) {
    g_controller = agent->GetTracingController();
  } else {
    g_controller = nullptr;
  }
}

TraceAgent* TraceEventHelper::GetTraceAgent() {
  return g_agent;
}

v8::TracingController* TraceEventHelper::GetTracingController() {
  return g_controller;
}

void TraceEventHelper::SetTracingController(v8::TracingController* controller) {
  g_controller = controller;
}

}  // namespace tracing

v8::TracingController* GetTracingController() {
  return tracing::TraceEventHelper::GetTracingController();
}

void SetTracingController(v8::TracingController* controller) {
  tracing::TraceEventHelper::SetTracingController(controller);
}

}  // namespace aworker
