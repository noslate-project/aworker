#include "inspector/inspector_io.h"
#include <condition_variable>  // NOLINT(build/c++11)
#include <cstring>
#include <deque>
#include <mutex>  // NOLINT(build/c++11)
#include <random>
#include <thread>  // NOLINT(build/c++11)
#include <vector>

#include "agent_channel/diag_channel.h"
#include "inspector/main_thread_interface.h"
#include "util.h"
#include "utils/async_primitives.h"
#include "v8-inspector.h"

namespace aworker {
namespace inspector {
namespace {
using v8_inspector::StringBuffer;
using v8_inspector::StringView;

// kKill closes connections and stops the server, kStop only stops the server
enum class TransportAction { kKill, kSendMessage, kStop };

// UUID RFC: https://www.ietf.org/rfc/rfc4122.txt
// Used ver 4 - with numbers
std::string GenerateID() {
  uint16_t buffer[8];
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib(INT_MIN, INT_MAX);
  for (int i = 0; i < 4; i++) {
    int g = distrib(gen);
    buffer[i * 2] = g & 0xFFFF;
    buffer[i * 2 + 1] = g >> 16;
  }

  char uuid[256];
  snprintf(uuid,
           sizeof(uuid),
           "%04x%04x-%04x-%04x-%04x-%04x%04x%04x",
           buffer[0],                      // time_low
           buffer[1],                      // time_mid
           buffer[2],                      // time_low
           (buffer[3] & 0x0fff) | 0x4000,  // time_hi_and_version
           (buffer[4] & 0x3fff) | 0x8000,  // clk_seq_hi clk_seq_low
           buffer[5],                      // node
           buffer[6],
           buffer[7]);
  return uuid;
}

class RequestToChannel {
 public:
  RequestToChannel(TransportAction action,
                   int session_id,
                   std::unique_ptr<v8_inspector::StringBuffer> message)
      : action_(action),
        session_id_(session_id),
        message_(std::move(message)) {}

  void Dispatch(AgentDiagChannel* channel) const {
    switch (action_) {
      case TransportAction::kKill:
        channel->TerminateConnections();
        // Fallthrough
      case TransportAction::kStop:
        channel->StopAcceptingNewConnections();
        break;
      case TransportAction::kSendMessage:
        channel->Send(session_id_, StringViewToUtf8(message_->string()));
        break;
    }
  }

 private:
  TransportAction action_;
  int session_id_;
  std::unique_ptr<v8_inspector::StringBuffer> message_;
};

class RequestQueue {
 public:
  using MessageQueue = std::deque<RequestToChannel>;
  using AsyncT = UvAsync<std::function<void()>>;

  explicit RequestQueue(uv_loop_t* loop)
      : handle_(std::make_shared<RequestQueueHandle>(this)) {
    async_ = AsyncT::Create(loop, [this]() { DoDispatch(); });
  }
  ~RequestQueue() = default;

  void Post(int session_id,
            TransportAction action,
            std::unique_ptr<StringBuffer> message) {
    ScopedLock scoped_lock(state_lock_);
    bool notify = messages_.empty();
    messages_.emplace_back(action, session_id, std::move(message));
    if (notify) {
      async_->Send();
      incoming_message_cond_.notify_all();
    }
  }

  void Wait() {
    UniqueLock lock(state_lock_);
    if (messages_.empty()) {
      incoming_message_cond_.wait(lock);
    }
  }

  void SetChannel(AgentDiagChannel* channel) { channel_ = channel; }

  std::shared_ptr<RequestQueueHandle> handle() { return handle_; }

 private:
  MessageQueue GetMessages() {
    ScopedLock scoped_lock(state_lock_);
    MessageQueue messages;
    messages_.swap(messages);
    return messages;
  }

  void DoDispatch() {
    if (channel_ == nullptr) return;
    for (const auto& request : GetMessages()) {
      request.Dispatch(channel_);
    }
  }

  std::shared_ptr<RequestQueueHandle> handle_;
  AsyncT::Ptr async_;
  AgentDiagChannel* channel_ = nullptr;
  MessageQueue messages_;
  std::mutex state_lock_;  // Locked before mutating the queue.
  std::condition_variable incoming_message_cond_;
};
}  // namespace

class RequestQueueHandle {
 public:
  explicit RequestQueueHandle(RequestQueue* data) : data_(data) {}

  void Reset() {
    ScopedLock scoped_lock(lock_);
    data_ = nullptr;
  }

  void Post(int session_id,
            TransportAction action,
            std::unique_ptr<StringBuffer> message) {
    ScopedLock scoped_lock(lock_);
    if (data_ != nullptr) data_->Post(session_id, action, std::move(message));
  }

  bool Expired() {
    ScopedLock scoped_lock(lock_);
    return data_ == nullptr;
  }

 private:
  RequestQueue* data_;
  std::mutex lock_;
};

class IoSessionDelegate : public InspectorSessionDelegate {
 public:
  explicit IoSessionDelegate(std::shared_ptr<RequestQueueHandle> queue, int id)
      : request_queue_(queue), id_(id) {}
  void SendMessageToFrontend(const v8_inspector::StringView& message) override {
    request_queue_->Post(
        id_, TransportAction::kSendMessage, StringBuffer::create(message));
  }

 private:
  std::shared_ptr<RequestQueueHandle> request_queue_;
  int id_;
};

// Passed to DiagChannelDelegate to handle inspector protocol events,
// mostly session start, message received, and session end.
class InspectorIoDelegate : public aworker::AgentDiagChannelInspectorDelegate {
 public:
  InspectorIoDelegate(std::shared_ptr<RequestQueue> queue,
                      std::shared_ptr<MainThreadHandle> main_thread,
                      const std::string& target_id,
                      const std::string& script_name);
  ~InspectorIoDelegate() override = default;

  void StartSession(int session_id, const std::string& target_id) override;
  void MessageReceived(int session_id, const std::string& message) override;
  void EndSession(int session_id) override;

  std::vector<std::string> GetTargetIds() override;
  std::string GetTargetTitle(const std::string& id) override;
  std::string GetTargetUrl(const std::string& id) override;
  void AssignDiagChannel(AgentDiagChannel* channel) override {
    request_queue_->SetChannel(channel);
  }

 private:
  std::shared_ptr<RequestQueue> request_queue_;
  std::shared_ptr<MainThreadHandle> main_thread_;
  std::unordered_map<int, std::unique_ptr<InspectorSession>> sessions_;
  const std::string script_name_;
  const std::string target_id_;
};

class InspectorIo::InspectorIoWatchdog : public WatchdogEntry {
 public:
  InspectorIoWatchdog(Watchdog* watchdog, InspectorIo* io) : io_(io) {
    watchdog->RegisterEntry(this);
  }

  void ThreadEntry(uv_loop_t* loop) override { io_->ThreadEntry(loop); }

  void ThreadAtExit() override { delete this; }

  InspectorIo* io_;
};

// static
std::unique_ptr<InspectorIo> InspectorIo::Start(
    Watchdog* watchdog,
    AgentDiagChannel* diag_channel,
    std::shared_ptr<MainThreadHandle> main_thread,
    const std::string& script_name) {
  auto io = std::unique_ptr<InspectorIo>(
      new InspectorIo(watchdog, diag_channel, main_thread, script_name));
  return io;
}

InspectorIo::InspectorIo(Watchdog* watchdog,
                         AgentDiagChannel* diag_channel,
                         std::shared_ptr<MainThreadHandle> main_thread,
                         const std::string& script_name)
    : diag_channel_(diag_channel),
      main_thread_(main_thread),
      script_name_(script_name),
      id_(GenerateID()) {
  new InspectorIoWatchdog(watchdog, this);
}

InspectorIo::~InspectorIo() {
  request_queue_->Post(0, TransportAction::kKill, nullptr);
}

void InspectorIo::StopAcceptingNewConnections() {
  request_queue_->Post(0, TransportAction::kStop, nullptr);
}

void InspectorIo::ThreadEntry(uv_loop_t* loop) {
  std::shared_ptr<RequestQueue> queue = std::make_shared<RequestQueue>(loop);
  request_queue_ = queue->handle();
  std::unique_ptr<InspectorIoDelegate> delegate =
      std::make_unique<InspectorIoDelegate>(
          std::move(queue), main_thread_, id_, script_name_);
  diag_channel_->AssignInspectorDelegate(move(delegate));
}

InspectorIoDelegate::InspectorIoDelegate(
    std::shared_ptr<RequestQueue> queue,
    std::shared_ptr<MainThreadHandle> main_thread,
    const std::string& target_id,
    const std::string& script_name)
    : request_queue_(queue),
      main_thread_(main_thread),
      script_name_(script_name),
      target_id_(target_id) {}

void InspectorIoDelegate::StartSession(int session_id,
                                       const std::string& target_id) {
  auto session = main_thread_->Connect(
      std::unique_ptr<InspectorSessionDelegate>(
          new IoSessionDelegate(request_queue_->handle(), session_id)),
      true);
  if (session) {
    sessions_[session_id] = std::move(session);
    fprintf(stderr, "Debugger attached.\n");
  }
}

void InspectorIoDelegate::MessageReceived(int session_id,
                                          const std::string& message) {
  auto session = sessions_.find(session_id);
  if (session != sessions_.end())
    session->second->Dispatch(Utf8ToStringBuffer(message)->string());
}

void InspectorIoDelegate::EndSession(int session_id) {
  sessions_.erase(session_id);
}

std::vector<std::string> InspectorIoDelegate::GetTargetIds() {
  return {target_id_};
}

std::string InspectorIoDelegate::GetTargetTitle(const std::string& id) {
  return script_name_.empty() ? "<unknown>.js" : script_name_;
}

std::string InspectorIoDelegate::GetTargetUrl(const std::string& id) {
  return "file://" + script_name_;
}

}  // namespace inspector
}  // namespace aworker
