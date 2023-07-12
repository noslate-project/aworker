#include "inspector/inspector_agent.h"
#include "immortal.h"
#include "inspector/main_thread_interface.h"
#include "libplatform/libplatform.h"
#include "v8-inspector.h"
#include "v8-platform.h"

#include "util.h"

#ifdef __POSIX__
#include <pthread.h>
#include <climits>  // PTHREAD_STACK_MIN
#endif              // __POSIX__

namespace aworker {
namespace inspector {

using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Message;
using v8::String;
using v8::Value;
using namespace v8_inspector;  // NOLINT(build/namespaces)

const int CONTEXT_GROUP_ID = 1;

std::unique_ptr<StringBuffer> ToProtocolString(Isolate* isolate,
                                               Local<Value> value) {
  String::Utf8Value utf8value(isolate, value);
  return StringBuffer::create(
      StringView(reinterpret_cast<uint8_t*>(*utf8value), utf8value.length()));
}

class ChannelImpl final : public v8_inspector::V8Inspector::Channel {
 public:
  explicit ChannelImpl(const std::unique_ptr<V8Inspector>& inspector,
                       std::unique_ptr<InspectorSessionDelegate> delegate,
                       std::shared_ptr<MainThreadHandle> main_thread_,
                       bool prevent_shutdown)
      : delegate_(std::move(delegate)),
        prevent_shutdown_(prevent_shutdown),
        retaining_context_(false) {
    session_ = inspector->connect(CONTEXT_GROUP_ID, this, StringView());
  }

  ~ChannelImpl() override {}

  void dispatchProtocolMessage(const StringView& message) {
    session_->dispatchProtocolMessage(message);
  }

  void schedulePauseOnNextStatement(const std::string& reason) {
    std::unique_ptr<StringBuffer> buffer = Utf8ToStringBuffer(reason);
    session_->schedulePauseOnNextStatement(buffer->string(), buffer->string());
  }

  bool preventShutdown() { return prevent_shutdown_; }

  bool retainingContext() { return retaining_context_; }

 private:
  void sendResponse(
      int callId,
      std::unique_ptr<v8_inspector::StringBuffer> message) override {
    sendMessageToFrontend(message->string());
  }

  void sendNotification(
      std::unique_ptr<v8_inspector::StringBuffer> message) override {
    sendMessageToFrontend(message->string());
  }

  void flushProtocolNotifications() override {}

  void sendMessageToFrontend(const StringView& message) {
    delegate_->SendMessageToFrontend(message);
  }

  std::unique_ptr<InspectorSessionDelegate> delegate_;
  std::unique_ptr<v8_inspector::V8InspectorSession> session_;
  bool prevent_shutdown_;
  bool retaining_context_;
};

class AworkerInspectorClient : public V8InspectorClient {
 public:
  AworkerInspectorClient(Immortal* immortal, std::string script_name)
      : immortal_(immortal) {
    inspector_ = V8Inspector::create(immortal_->isolate(), this);
    ContextInfo info{script_name, "", true};
    HandleScope scope(immortal_->isolate());
    contextCreated(immortal->context(), info);
  }

  void runMessageLoopOnPause(int context_group_id) override {
    waiting_for_resume_ = true;
    runMessageLoop();
  }

  void waitForSessionsDisconnect() {
    waiting_for_sessions_disconnect_ = true;
    runMessageLoop();
  }

  void waitForFrontend() {
    waiting_for_frontend_ = true;
    runMessageLoop();
  }

  void contextCreated(Local<Context> context, const ContextInfo& info) {
    auto name_buffer = Utf8ToStringBuffer(info.name);
    auto origin_buffer = Utf8ToStringBuffer(info.origin);
    std::unique_ptr<StringBuffer> aux_data_buffer;

    v8_inspector::V8ContextInfo v8info(
        context, CONTEXT_GROUP_ID, name_buffer->string());
    v8info.origin = origin_buffer->string();

    if (info.is_default) {
      aux_data_buffer = Utf8ToStringBuffer("{\"isDefault\":true}");
    } else {
      aux_data_buffer = Utf8ToStringBuffer("{\"isDefault\":false}");
    }
    v8info.auxData = aux_data_buffer->string();

    inspector_->contextCreated(v8info);
  }

  void contextDestroyed(Local<Context> context) {
    inspector_->contextDestroyed(context);
  }

  void quitMessageLoopOnPause() override { waiting_for_resume_ = false; }

  void runIfWaitingForDebugger(int context_group_id) override {
    waiting_for_frontend_ = false;
    if (run_message_loop_async_ != nullptr) run_message_loop_async_->Send();
  }

  int connectFrontend(std::unique_ptr<InspectorSessionDelegate> delegate,
                      bool prevent_shutdown) {
    int session_id = next_session_id_++;
    channels_[session_id] = std::make_unique<ChannelImpl>(
        inspector_, std::move(delegate), getThreadHandle(), prevent_shutdown);
    return session_id;
  }

  void disconnectFrontend(int session_id) {
    auto it = channels_.find(session_id);
    if (it == channels_.end()) return;
    bool retaining_context = it->second->retainingContext();
    channels_.erase(it);
    if (retaining_context) {
      for (const auto& id_channel : channels_) {
        if (id_channel.second->retainingContext()) return;
      }
      contextDestroyed(immortal_->context());
    }
    if (waiting_for_sessions_disconnect_)
      waiting_for_sessions_disconnect_ = false;
  }

  void dispatchMessageFromFrontend(int session_id, const StringView& message) {
    channels_[session_id]->dispatchProtocolMessage(message);
  }

  Local<Context> ensureDefaultContextInGroup(int contextGroupId) override {
    return immortal_->context();
  }

  std::unique_ptr<StringBuffer> resourceNameToUrl(
      const StringView& resource_name_view) override {
    std::string resource_name = StringViewToUtf8(resource_name_view);
    if (resource_name[0] == '/') {
      return Utf8ToStringBuffer("file://" + resource_name);
    }
    return nullptr;
  }

  // TODO(chengzhong.wcz): impl
  void ReportUncaughtException(Local<Value> error, Local<Message> message) {
    Isolate* isolate = immortal_->isolate();
    Local<Context> context = immortal_->context();

    int script_id = message->GetScriptOrigin().ScriptId();

    Local<v8::StackTrace> stack_trace = message->GetStackTrace();

    if (!stack_trace.IsEmpty() && stack_trace->GetFrameCount() > 0 &&
        script_id == stack_trace->GetFrame(isolate, 0)->GetScriptId()) {
      script_id = 0;
    }

    const uint8_t DETAILS[] = "Uncaught";

    inspector_->exceptionThrown(
        context,
        StringView(DETAILS, sizeof(DETAILS) - 1),
        error,
        ToProtocolString(isolate, message->Get())->string(),
        ToProtocolString(isolate, message->GetScriptResourceName())->string(),
        message->GetLineNumber(context).FromMaybe(0),
        message->GetStartColumn(context).FromMaybe(0),
        inspector_->createStackTrace(stack_trace),
        script_id);
  }

  void schedulePauseOnNextStatement(const std::string& reason) {
    for (const auto& id_channel : channels_) {
      id_channel.second->schedulePauseOnNextStatement(reason);
    }
  }

  bool hasConnectedSessions() {
    for (const auto& id_channel : channels_) {
      // Other sessions are "invisible" more most purposes
      if (id_channel.second->preventShutdown()) return true;
    }
    return false;
  }

  std::shared_ptr<MainThreadHandle> getThreadHandle() {
    if (!interface_) {
      interface_ = std::make_shared<MainThreadInterface>(
          immortal_->inspector_agent().get());
    }
    return interface_->GetHandle();
  }

  bool IsActive() { return !channels_.empty(); }

 private:
  bool shouldRunMessageLoop() {
    if (waiting_for_frontend_) return true;
    if (waiting_for_sessions_disconnect_ || waiting_for_resume_) {
      return hasConnectedSessions();
    }
    return false;
  }

  void runMessageLoop() {
    if (running_nested_loop_) return;

    running_nested_loop_ = true;
    uv_loop_t* loop = immortal_->event_loop();

    run_message_loop_async_ = UvAsync<std::function<void()>>::Create(
        loop, std::bind(&AworkerInspectorClient::RunMessageLoopAsyncCb, this));
    while (shouldRunMessageLoop() && uv_loop_alive(loop) != 0) {
      if (interface_) interface_->WaitForFrontendEvent();
      uv_run(loop, UV_RUN_ONCE);
    }
    run_message_loop_async_.reset();
    running_nested_loop_ = false;
  }

  void RunMessageLoopAsyncCb() {}

  Immortal* immortal_;
  bool running_nested_loop_ = false;
  std::unique_ptr<V8Inspector> inspector_;
  std::unordered_map<int, std::unique_ptr<ChannelImpl>> channels_;
  int next_session_id_ = 1;
  bool waiting_for_resume_ = false;
  bool waiting_for_frontend_ = false;
  bool waiting_for_sessions_disconnect_ = false;
  // Allows accessing Inspector from non-main threads
  std::shared_ptr<MainThreadInterface> interface_;
  UvAsync<std::function<void()>>::Ptr run_message_loop_async_;
};

class SameThreadInspectorSession : public InspectorSession {
 public:
  SameThreadInspectorSession(int session_id,
                             std::shared_ptr<AworkerInspectorClient> client)
      : session_id_(session_id), client_(client) {}
  ~SameThreadInspectorSession() override;
  void Dispatch(const v8_inspector::StringView& message) override;

 private:
  int session_id_;
  std::weak_ptr<AworkerInspectorClient> client_;
};

InspectorAgent::InspectorAgent(Immortal* immortal) : immortal_(immortal) {}

InspectorAgent::~InspectorAgent() {}

bool InspectorAgent::Start(const std::string& script_name) {
  if (client_ != nullptr) return true;
  script_name_ = script_name;

  client_ = std::make_shared<AworkerInspectorClient>(immortal_, script_name);
  return true;
}

bool InspectorAgent::StartInspectorIo() {
  if (io_ != nullptr) return true;

  CHECK_NOT_NULL(client_);

  io_ = InspectorIo::Start(immortal_->watchdog(),
                           immortal_->agent_diag_channel().get(),
                           client_->getThreadHandle(),
                           script_name_);
  if (io_ == nullptr) {
    return false;
  }
  return true;
}

void InspectorAgent::Stop() {
  io_.reset();
  client_.reset();
}

void InspectorAgent::StopInspectorIo() {
  io_.reset();
}

bool InspectorAgent::HasConnectedSessions() {
  return client_->hasConnectedSessions();
}

void InspectorAgent::WaitForDisconnect() {
  CHECK_NOT_NULL(client_);
  if (client_->hasConnectedSessions()) {
    fprintf(stderr, "Waiting for the debugger to disconnect...\n");
    fflush(stderr);
  }
  if (io_ != nullptr) {
    io_->StopAcceptingNewConnections();
    client_->waitForSessionsDisconnect();
  }
}

void InspectorAgent::ReportUncaughtException(Local<Value> error,
                                             Local<Message> message) {
  if (!IsActive()) return;
  client_->ReportUncaughtException(error, message);
  WaitForDisconnect();
}

std::unique_ptr<InspectorSession> InspectorAgent::Connect(
    std::unique_ptr<InspectorSessionDelegate> delegate, bool prevent_shutdown) {
  CHECK_NOT_NULL(client_);
  int session_id =
      client_->connectFrontend(std::move(delegate), prevent_shutdown);
  return std::unique_ptr<InspectorSession>(
      new SameThreadInspectorSession(session_id, client_));
}

void InspectorAgent::PauseOnNextJavascriptStatement(const std::string& reason) {
  client_->schedulePauseOnNextStatement(reason);
}

void InspectorAgent::ContextCreated(Local<Context> context,
                                    const ContextInfo& info) {
  if (client_ == nullptr)  // This happens for a main context
    return;
  client_->contextCreated(context, info);
}

bool InspectorAgent::WillWaitForConnect() {
  return true;
}

bool InspectorAgent::IsActive() {
  if (client_ == nullptr) return false;
  return io_ != nullptr || client_->IsActive();
}

void InspectorAgent::WaitForConnect() {
  CHECK_NOT_NULL(client_);
  fprintf(stderr, "Waiting for the debugger to connect...\n");
  fflush(stderr);
  client_->waitForFrontend();
}

SameThreadInspectorSession::~SameThreadInspectorSession() {
  auto client = client_.lock();
  if (client) client->disconnectFrontend(session_id_);
}

void SameThreadInspectorSession::Dispatch(
    const v8_inspector::StringView& message) {
  auto client = client_.lock();
  if (client) client->dispatchMessageFromFrontend(session_id_, message);
}

}  // namespace inspector
}  // namespace aworker
