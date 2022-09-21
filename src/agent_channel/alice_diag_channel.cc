#include "agent_channel/alice_diag_channel.h"
#include "aworker_logger.h"
#include "debug_utils.h"
#include "immortal.h"
#include "inspector/inspector_agent.h"
#include "tracing/trace_agent.h"
#include "tracing/trace_event.h"

namespace aworker {
namespace agent {

using std::unique_ptr;

AliceDiagChannel::AliceDiagChannel(Immortal* immortal,
                                   std::string server_path,
                                   std::string credential)
    : AgentDiagChannel(immortal->watchdog(), credential),
      AliceService(),
      immortal_(immortal),
      server_path_(server_path) {}

AliceDiagChannel::~AliceDiagChannel() {}

void AliceDiagChannel::Start(uv_loop_t* loop) {
  per_process::Debug(DebugCategory::AGENT_CHANNEL, "start diag channel\n");
  loop_ = loop;
  auto loop_handle = std::make_shared<UvLoop>(loop_);
  socket_delegate_ =
      std::make_shared<ClientDelegate>(unowned_ptr(this), loop_handle);
  UvSocketHolder::Connect(
      loop_handle,
      server_path_,
      socket_delegate_,
      std::bind(&AliceDiagChannel::OnConnect, this, std::placeholders::_1));
}

void AliceDiagChannel::AssignInspectorDelegate(
    std::unique_ptr<AgentDiagChannelInspectorDelegate> delegate) {
  per_process::Debug(DebugCategory::AGENT_CHANNEL,
                     "diagnostics channel assigned inspector delegate\n");
  delegate_ = std::move(delegate);
  delegate_->AssignDiagChannel(this);
  if (connected_) {
    SendInspectorStarted();
  }
}

void AliceDiagChannel::Stop() {
  connected_ = true;
  socket_.reset();
}

void AliceDiagChannel::TerminateConnections() {
  delegate_.reset();
}

void AliceDiagChannel::StopAcceptingNewConnections() {
  stopping_ = true;
}

void AliceDiagChannel::Send(int session_id, std::string message) {
  auto controller = NewControllerWithTimeout(20000);
  auto msg = std::make_unique<InspectorEventRequestMessage>();
  msg->set_session_id(session_id);
  msg->set_message(message);
  per_process::Debug(DebugCategory::AGENT_CHANNEL,
                     "Dispatch inspector event %d\n",
                     controller->request_id());
  Request(move(controller),
          move(msg),
          [](CanonicalCode code,
             unique_ptr<ErrorResponseMessage> error,
             unique_ptr<InspectorEventResponseMessage> msg) {
            per_process::Debug(DebugCategory::AGENT_CHANNEL,
                               "Dispatch inspector event code: %d\n",
                               code);
          });
}

void AliceDiagChannel::InspectorStart(
    unique_ptr<RpcController> controller,
    const unique_ptr<InspectorStartRequestMessage> req,
    unique_ptr<InspectorStartResponseMessage> res,
    Closure<InspectorStartResponseMessage> closure) {
  per_process::Debug(DebugCategory::AGENT_CHANNEL,
                     "diagnostics channel inspector start\n");
  if (stopping_) {
    auto err = std::make_unique<ErrorResponseMessage>();
    err->set_message("Inspector channel closing.");
    closure(CanonicalCode::CLIENT_ERROR, std::move(err), nullptr);
    return;
  }
  immortal_->RequestInterrupt([this, closure]() {
    per_process::Debug(DebugCategory::AGENT_CHANNEL,
                       "diagnostics channel inspector start interrupted\n");

    // TODO(chengzhong.wcz): get main context script name.
    immortal_->inspector_agent()->Start("");

    closure(CanonicalCode::OK,
            nullptr,
            std::make_unique<InspectorStartResponseMessage>());
  });
}

void AliceDiagChannel::InspectorStartSession(
    unique_ptr<RpcController> controller,
    const unique_ptr<InspectorStartSessionRequestMessage> req,
    unique_ptr<InspectorStartSessionResponseMessage> res,
    Closure<InspectorStartSessionResponseMessage> closure) {
  if (stopping_) {
    auto err = std::make_unique<ErrorResponseMessage>();
    err->set_message("Inspector channel closing.");
    closure(CanonicalCode::CLIENT_ERROR, std::move(err), nullptr);
    return;
  }
  if (delegate_ == nullptr) {
    auto err = std::make_unique<ErrorResponseMessage>();
    err->set_message("Inspector channel not started.");
    closure(CanonicalCode::CLIENT_ERROR, std::move(err), nullptr);
    return;
  }
  delegate_->StartSession(req->session_id(), req->target_id());
  opened_session_count_++;
  closure(CanonicalCode::OK, nullptr, std::move(res));
}

void AliceDiagChannel::InspectorEndSession(
    unique_ptr<RpcController> controller,
    const unique_ptr<InspectorEndSessionRequestMessage> req,
    unique_ptr<InspectorEndSessionResponseMessage> res,
    Closure<InspectorEndSessionResponseMessage> closure) {
  if (delegate_ == nullptr) {
    auto err = std::make_unique<ErrorResponseMessage>();
    err->set_message("Inspector channel not started.");
    closure(CanonicalCode::CLIENT_ERROR, std::move(err), nullptr);
    return;
  }
  if (opened_session_count_ > 0) {
    delegate_->EndSession(req->session_id());
    opened_session_count_--;
  }
  closure(CanonicalCode::OK, nullptr, std::move(res));
}

void AliceDiagChannel::InspectorGetTargets(
    unique_ptr<RpcController> controller,
    const unique_ptr<InspectorGetTargetsRequestMessage> req,
    unique_ptr<InspectorGetTargetsResponseMessage> res,
    Closure<InspectorGetTargetsResponseMessage> closure) {
  per_process::Debug(DebugCategory::AGENT_CHANNEL,
                     "diagnostics channel inspector get targets\n");
  if (delegate_ == nullptr) {
    auto err = std::make_unique<ErrorResponseMessage>();
    err->set_message("Inspector channel not started.");
    closure(CanonicalCode::CLIENT_ERROR, std::move(err), nullptr);
    return;
  }
  auto ids = delegate_->GetTargetIds();
  for (auto id : ids) {
    auto target = res->add_targets();
    auto title = delegate_->GetTargetTitle(id);
    auto url = delegate_->GetTargetUrl(id);
    target->set_id(id);
    target->set_title(title);
    target->set_url(url);
  }
  closure(CanonicalCode::OK, nullptr, std::move(res));
}

void AliceDiagChannel::InspectorCommand(
    unique_ptr<RpcController> controller,
    const unique_ptr<InspectorCommandRequestMessage> req,
    unique_ptr<InspectorCommandResponseMessage> res,
    Closure<InspectorCommandResponseMessage> closure) {
  per_process::Debug(DebugCategory::AGENT_CHANNEL,
                     "diagnostics channel inspector command\n");
  if (delegate_ == nullptr) {
    auto err = std::make_unique<ErrorResponseMessage>();
    err->set_message("Inspector channel not started.");
    closure(CanonicalCode::CLIENT_ERROR, std::move(err), nullptr);
    return;
  }
  delegate_->MessageReceived(req->session_id(), req->message());
  closure(CanonicalCode::OK, nullptr, std::move(res));
}

void AliceDiagChannel::TracingStart(
    unique_ptr<RpcController> controller,
    unique_ptr<TracingStartRequestMessage> req,
    unique_ptr<TracingStartResponseMessage> res,
    Closure<TracingStartResponseMessage> closure) {
  per_process::Debug(DebugCategory::AGENT_CHANNEL,
                     "diagnostics channel tracing start\n");
  immortal_->RequestInterrupt([req = req.release(), closure]() {
    per_process::Debug(DebugCategory::AGENT_CHANNEL,
                       "diagnostics channel tracing start interrupted\n");
    TraceAgent* trace_agent =
        aworker::tracing::TraceEventHelper::GetTraceAgent();
    TraceAgent::SuspendTracingScope suspend(trace_agent);

    for (uint32_t idx = 0; idx < req->categories_size(); idx++) {
      trace_agent->Enable(req->categories(idx));
    }

    delete req;
    closure(CanonicalCode::OK,
            nullptr,
            std::make_unique<TracingStartResponseMessage>());
  });
}

void AliceDiagChannel::TracingStop(
    unique_ptr<RpcController> controller,
    unique_ptr<TracingStopRequestMessage> req,
    unique_ptr<TracingStopResponseMessage> res,
    Closure<TracingStopResponseMessage> closure) {
  per_process::Debug(DebugCategory::AGENT_CHANNEL,
                     "diagnostics channel tracing stop\n");
  immortal_->RequestInterrupt([closure]() {
    TraceAgent* trace_agent =
        aworker::tracing::TraceEventHelper::GetTraceAgent();
    trace_agent->Stop();
    closure(CanonicalCode::OK,
            nullptr,
            std::make_unique<TracingStopResponseMessage>());
  });
}

void AliceDiagChannel::Disconnected(SessionId session_id) {
  per_process::Debug(DebugCategory::AGENT_CHANNEL,
                     "diagnostics channel disconnected\n");
  socket_.reset();
}

void AliceDiagChannel::OnError() {
  ELOG("diagnostics channel error");
}

void AliceDiagChannel::SendInspectorStarted() {
  auto controller = NewControllerWithTimeout(20000);
  auto msg = std::make_unique<InspectorStartedRequestMessage>();
  per_process::Debug(DebugCategory::AGENT_CHANNEL,
                     "Dispatch inspector started %d\n",
                     controller->request_id());
  Request(move(controller),
          move(msg),
          [](CanonicalCode code,
             unique_ptr<ErrorResponseMessage> error,
             unique_ptr<InspectorStartedResponseMessage> msg) {
            per_process::Debug(DebugCategory::AGENT_CHANNEL,
                               "Dispatch inspector started code: %d\n",
                               code);
          });
}

void AliceDiagChannel::OnConnect(UvSocketHolder::Pointer socket) {
  per_process::Debug(DebugCategory::AGENT_CHANNEL,
                     "diagnostics channel on connect\n");
  socket_ = std::move(socket);
  if (socket_ != nullptr) {
    per_process::Debug(DebugCategory::AGENT_CHANNEL,
                       "binding diagnostics credentials\n");
    auto controller = NewControllerWithTimeout(1000);
    auto msg = std::make_unique<CredentialsRequestMessage>();
    msg->set_cred(credential());
    msg->set_type(CredentialTargetType::Diagnostics);
    this->AliceService::Request(
        move(controller),
        move(msg),
        [this](CanonicalCode code,
               unique_ptr<ErrorResponseMessage> error,
               unique_ptr<CredentialsResponseMessage> msg) {
          if (code != CanonicalCode::OK) {
            ELOG("Agent rejected diagnostics credential with code(%d)", code);
            socket_.reset();
            return;
          }
          connected_ = true;
          if (delegate_) {
            SendInspectorStarted();
          }
        });
  }
}
}  // namespace agent
}  // namespace aworker
