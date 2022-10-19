#include "ipc/ipc_socket.h"
#include <string>
#include "aworker_logger.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_pb.h"
#include "util.h"

namespace aworker {
namespace ipc {

using std::unique_ptr;

void ProtobufLogHandler(google::protobuf::LogLevel level,
                        const char* filename,
                        int line,
                        const std::string& message) {
  if (level < google::protobuf::LOGLEVEL_WARNING) {
    _LOG(stdout,
         LogLevel::kInfo,
         LOG_LEVEL_INFO_TAG,
         filename,
         line,
         "%s",
         message.c_str());
    return;
  }
  _LOG(stderr,
       LogLevel::kError,
       LOG_LEVEL_ERROR_TAG,
       filename,
       line,
       "%s",
       message.c_str());
}

void NoslatedDecoder::Init() {
  google::protobuf::SetLogHandler(ProtobufLogHandler);
}

NoslatedDecoder::NoslatedDecoder() {}

void NoslatedDecoder::InsertBuffer(const char* base, size_t len) {
  DLOG("NoslatedDecoder::InsertBuffer(%p) %zu", this, len);
  if (len > 0) {
    buffer_.insert(buffer_.end(), base, base + len);
  }
}

DecodingState NoslatedDecoder::Decode() {
  if (buffer_.size() < MessageHeaderSize) {
    DLOG("NoslatedDecoder::Decode(%p) buffer size less than header: %zu",
         this,
         buffer_.size());
    return DecodingState::DecodingStateMore;
  }
  if (header_ == nullptr) {
    auto header = std::make_unique<MessageHeader>();
    if (!header->ParseFromArray(buffer_.cbegin().base(), MessageHeaderSize)) {
      DLOG("parse header failed");
      return DecodingState::DecodingStateError;
    }
    header_ = std::move(header);
    read_size_ = MessageHeaderSize;
  }
  if (buffer_.size() < (MessageHeaderSize + header_->content_length())) {
    DLOG("NoslatedDecoder::Decode buffer size less than content: %zu, wanted: "
         "%zu",
         buffer_.size(),
         (MessageHeaderSize + header_->content_length()));
    return DecodingState::DecodingStateMore;
  }

  switch (header_->message_kind()) {
    case MessageKind::Request: {
      return DecodeRequest();
    }
    case MessageKind::Response: {
      return DecodeResponse();
    }
    default:
      DCHECK(false);
  }
  DLOG("parse body failed");
  return DecodingState::DecodingStateError;
}

DecodingState NoslatedDecoder::DecodeRequest() {
#define V(TYPE)                                                                \
  case RequestKind::TYPE: {                                                    \
    auto msg = new TYPE##RequestMessage();                                     \
    if (!msg->ParseFromArray(buffer_.cbegin().base() + MessageHeaderSize,      \
                             header_->content_length())) {                     \
      DLOG("parse request body(RequestKind::" #TYPE ") failed");               \
      return DecodingState::DecodingStateError;                                \
    }                                                                          \
    read_size_ += header_->content_length();                                   \
    content_ = msg;                                                            \
    break;                                                                     \
  }
  switch (header_->request_kind()) {
    NOSLATED_REQUEST_TYPES(V)
    default:
      DCHECK(false);
      DLOG("unrecognizable request body with request kind %d",
           header_->request_kind());
      return DecodingState::DecodingStateError;
  }
#undef V
  return DecodingState::DecodingStateOk;
}

DecodingState NoslatedDecoder::DecodeResponse() {
  if (static_cast<CanonicalCode>(header_->code()) != CanonicalCode::OK) {
    auto msg = new ErrorResponseMessage();
    if (!msg->ParseFromArray(buffer_.cbegin().base() + MessageHeaderSize,
                             header_->content_length())) {
      DLOG("parse error response failed");
      return DecodingState::DecodingStateError;
    }
    read_size_ += header_->content_length();
    content_ = msg;
    return DecodingState::DecodingStateOk;
  }
#define V(TYPE)                                                                \
  case RequestKind::TYPE: {                                                    \
    auto msg = new TYPE##ResponseMessage();                                    \
    if (!msg->ParseFromArray(buffer_.cbegin().base() + MessageHeaderSize,      \
                             header_->content_length())) {                     \
      DLOG("parse response body(RequestKind::" #TYPE ") failed");              \
      return DecodingState::DecodingStateError;                                \
    }                                                                          \
    read_size_ += header_->content_length();                                   \
    content_ = msg;                                                            \
    break;                                                                     \
  }
  switch (header_->request_kind()) {
    NOSLATED_REQUEST_TYPES(V)
    default:
      DCHECK(false);
      DLOG("unrecognizable response body with request kind %d",
           header_->request_kind());
      return DecodingState::DecodingStateError;
  }
#undef V
  return DecodingState::DecodingStateOk;
}

Message* NoslatedDecoder::TakeContentOwnership() {
  DLOG("NoslatedDecoder::TakeContentOwnership: %zu, read: %zu",
       buffer_.size(),
       read_size_);
  Message* content = content_;
  content_ = nullptr;
  buffer_ = std::vector<uint8_t>(buffer_.begin() + read_size_, buffer_.end());
  read_size_ = 0;
  header_ = nullptr;
  return content;
}

void SocketHolder::OnFrame(const char* base, size_t len) {
  DLOG("read data: %zu", len);
  decoder_.InsertBuffer(base, len);
  SetReadable();
}

void SocketHolder::OnEoF() {
  auto state = decoder_.Decode();
  if (state == DecodingState::DecodingStateError) {
    delegate_->OnError();
    return;
  }
  if (state == DecodingState::DecodingStateOk) {
    SocketHolder::Dispatch();
  }
  delegate_->OnFinished();
}

void SocketHolder::DisconnectAndDispose() {
  if (read_immediate_ != nullptr) {
    loop_->ClearImmediate(read_immediate_);
    read_immediate_ = nullptr;
  }
}

void SocketHolder::Dispatch() {
  std::unique_ptr<MessageHeader> header = decoder_.TakeHeaderOwnership();
  DLOG("dispatching message kind: %d", header->message_kind());
  switch (header->message_kind()) {
    case MessageKind::Request:
      delegate_->OnRequest(
          header->request_id(),
          static_cast<RequestKind>(header->request_kind()),
          unique_ptr<Message>(decoder_.TakeContentOwnership()));
      break;
    case MessageKind::Response:
      delegate_->OnResponse(
          header->request_id(),
          static_cast<RequestKind>(header->request_kind()),
          static_cast<CanonicalCode>(header->code()),
          unique_ptr<Message>(decoder_.TakeContentOwnership()));
      break;
    default:
      DCHECK(false);
  }
}

void SocketHolder::SetReadable() {
  if (read_immediate_ != nullptr) {
    return;
  }
  read_immediate_ = loop_->SetImmediate([this](Immediate* immediate) {
    loop_->ClearImmediate(immediate);
    read_immediate_ = nullptr;
    OnReadable();
  });
}

void SocketHolder::OnReadable() {
  DLOG("socket immediate callback");
  while (true) {
    auto state = decoder_.Decode();
    DLOG("read data state: %d", state);
    if (state == DecodingState::DecodingStateError) {
      delegate_->OnError();
      return;
    }
    if (state == DecodingState::DecodingStateOk) {
      SocketHolder::Dispatch();
    }
    if (state == DecodingState::DecodingStateMore) {
      break;
    }
  }
}

void SocketHolder::Write(MessageKind mkind,
                         RequestId rid,
                         RequestKind rkind,
                         unique_ptr<Message> msg,
                         CanonicalCode code) {
  size_t msg_size = msg->ByteSize();
  MessageHeader header;
  header.set_message_kind(mkind);
  header.set_request_id(rid);
  header.set_request_kind(rkind);
  header.set_content_length(msg_size);
  header.set_code(code);
  size_t header_size = header.ByteSize();
  size_t data_size = header_size + msg_size;

  DLOG("send message(req_id: %u) data length %zu, header "
       "%zu, req %zu",
       rid,
       data_size,
       header_size,
       msg_size);
  char* data = new char[data_size];
  header.SerializeToArray(data, header_size);
  msg->SerializeToArray(data + header_size, msg_size);
  Write(data, data_size);
}

}  // namespace ipc
}  // namespace aworker
