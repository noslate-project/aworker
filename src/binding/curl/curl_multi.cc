#include "binding/curl/curl_multi.h"
#include "binding/curl/curl_easy.h"
#include "util.h"

#include "error_handling.h"

namespace aworker {
namespace curl {
using v8::Context;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;

const WrapperTypeInfo CurlMulti::wrapper_type_info_{
    "curl_multi",
};

AWORKER_BINDING(CurlMulti::Initialize) {
  Local<FunctionTemplate> tpl =
      FunctionTemplate::New(immortal->isolate(), CurlMulti::New);
  tpl->Inherit(AsyncWrap::GetConstructorTemplate(immortal));
  tpl->InstanceTemplate()->SetInternalFieldCount(
      BaseObject::kInternalFieldCount);

  Local<String> name = OneByteString(immortal->isolate(), "CurlMulti");
  tpl->SetClassName(name);

  Local<ObjectTemplate> prototype_template = tpl->PrototypeTemplate();
  immortal->SetFunctionProperty(prototype_template, "addHandle", AddHandle);
  immortal->SetFunctionProperty(
      prototype_template, "removeHandle", RemoveHandle);
  immortal->SetFunctionProperty(prototype_template, "close", Close);

  exports->Set(context, name, tpl->GetFunction(context).ToLocalChecked())
      .Check();
}

AWORKER_EXTERNAL_REFERENCE(CurlMulti::Initialize) {
  registry->Register(New);
  registry->Register(AddHandle);
  registry->Register(RemoveHandle);
  registry->Register(Close);
}

AWORKER_METHOD(CurlMulti::New) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  new CurlMulti(immortal, info.This());

  info.GetReturnValue().Set(info.This());
}

AWORKER_METHOD(CurlMulti::AddHandle) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  CurlMulti* cm;
  ASSIGN_OR_RETURN_UNWRAP(&cm, info.This());

  CurlEasy* ce;
  ASSIGN_OR_RETURN_UNWRAP(&ce, info[0]);

  if (cm->pending_handles_.find(ce) != cm->pending_handles_.cend()) {
    ThrowException(isolate, "handle is pending");
    return;
  }

  CURLMcode code = curl_multi_add_handle(cm->multi_handle_, ce->easy_handle());
  if (code != CURLMcode::CURLM_OK) {
    ThrowException(isolate, curl_multi_strerror(code));
    return;
  }
  cm->pending_handles_.insert(ce);
  ce->ClearWeak();
}

AWORKER_METHOD(CurlMulti::RemoveHandle) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  CurlMulti* cm;
  ASSIGN_OR_RETURN_UNWRAP(&cm, info.This());

  CurlEasy* ce;
  ASSIGN_OR_RETURN_UNWRAP(&ce, info[0]);

  if (cm->pending_handles_.find(ce) == cm->pending_handles_.cend()) {
    ThrowException(isolate, "handle is pending");
    return;
  }

  CURLMcode code =
      curl_multi_remove_handle(cm->multi_handle_, ce->easy_handle());
  if (code != CURLMcode::CURLM_OK) {
    ThrowException(isolate, curl_multi_strerror(code));
    return;
  }
  cm->pending_handles_.erase(ce);
  ce->MakeWeak();
}

AWORKER_METHOD(CurlMulti::Close) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  CurlMulti* cm;
  ASSIGN_OR_RETURN_UNWRAP(&cm, info.This());

  uv_timer_stop(&cm->timer_);
  uv_close(reinterpret_cast<uv_handle_t*>(&cm->timer_), OnClose);
}

void CurlMulti::OnPoll(CurlMultiContext* ctx, int flags) {
  int running_handles;
  curl_multi_socket_action(
      multi_handle_, ctx->sockfd(), flags, &running_handles);
  ProcessMessages();
}

// static
void CurlMulti::OnTimeout(uv_timer_t* req) {
  CurlMulti* cm = ContainerOf(&CurlMulti::timer_, req);

  int running_handles;
  curl_multi_socket_action(
      cm->multi_handle_, CURL_SOCKET_TIMEOUT, 0, &running_handles);
  cm->ProcessMessages();
}

// static
void CurlMulti::OnClose(uv_handle_t* handle) {
  CurlMulti* multi =
      ContainerOf(&CurlMulti::timer_, reinterpret_cast<uv_timer_t*>(handle));
  delete multi;
}

void CurlMulti::ProcessMessages() {
  CURLMsg* msg = NULL;
  int pending = 0;

  while ((msg = curl_multi_info_read(multi_handle_, &pending))) {
    if (msg->msg == CURLMSG_DONE) {
      CURLcode code = msg->data.result;

      CurlEasy* ce = CurlEasy::FromEasyHandle(msg->easy_handle);
      ce->OnDone(code);
    }
  }
}

// static
int CurlMulti::HandleSocket(
    CURL* easy, curl_socket_t socket, int action, void* userp, void* socketp) {
  CurlMulti* cm = static_cast<CurlMulti*>(userp);
  CurlMultiContext* curl_context = static_cast<CurlMultiContext*>(socketp);
  int events = 0;

  switch (action) {
    case CURL_POLL_IN:
    case CURL_POLL_OUT:
    case CURL_POLL_INOUT:
      if (curl_context == nullptr) {
        curl_context = CurlMultiContext::Create(cm, socket);
      }

      curl_multi_assign(
          cm->multi_handle_, socket, static_cast<void*>(curl_context));

      if (action != CURL_POLL_IN) events |= UV_WRITABLE;
      if (action != CURL_POLL_OUT) events |= UV_READABLE;

      curl_context->Start(events);
      break;
    case CURL_POLL_REMOVE:
      if (curl_context != nullptr) {
        curl_context->Destroy();
        curl_multi_assign(cm->multi_handle_, socket, nullptr);
      }
      break;
    default:
      UNREACHABLE();
  }

  return 0;
}

// static
int CurlMulti::StartTimeout(CURLM* multi, int64_t timeout_ms, void* userp) {
  CurlMulti* cm = static_cast<CurlMulti*>(userp);

  if (timeout_ms < 0) {
    uv_timer_stop(&cm->timer_);
    return 0;
  }

  // 0 means directly call socket_action, but we will do it in a bit.
  if (timeout_ms == 0) timeout_ms = 1;
  uv_timer_start(&cm->timer_, OnTimeout, timeout_ms, 0);
  return 0;
}

CurlMulti::CurlMulti(Immortal* immortal, Local<Object> object)
    : AsyncWrap(immortal, object), loop_(immortal->event_loop()) {
  CHECK_EQ(uv_timer_init(loop_, &timer_), 0);
  multi_handle_ = curl_multi_init();

  curl_multi_setopt(multi_handle_, CURLMOPT_SOCKETDATA, this);
  curl_multi_setopt(multi_handle_, CURLMOPT_SOCKETFUNCTION, HandleSocket);

  curl_multi_setopt(multi_handle_, CURLMOPT_TIMERDATA, this);
  curl_multi_setopt(multi_handle_, CURLMOPT_TIMERFUNCTION, StartTimeout);
}

CurlMulti::~CurlMulti() {
  curl_multi_cleanup(multi_handle_);
}

// static
CurlMultiContext* CurlMultiContext::Create(CurlMulti* multi,
                                           curl_socket_t sockfd_) {
  return new CurlMultiContext(multi, sockfd_);
}

void CurlMultiContext::Destroy() {
  uv_poll_stop(&poll_);
  uv_close(reinterpret_cast<uv_handle_t*>(&poll_), OnClose);
}

void CurlMultiContext::Start(int events) {
  uv_poll_start(&poll_, events, OnPoll);
}

// static
void CurlMultiContext::OnPoll(uv_poll_t* req, int status, int events) {
  CurlMultiContext* context = ContainerOf(&CurlMultiContext::poll_, req);
  int flags = 0;

  if (events & UV_READABLE) flags |= CURL_CSELECT_IN;
  if (events & UV_WRITABLE) flags |= CURL_CSELECT_OUT;

  context->multi_->OnPoll(context, flags);
}

// static
void CurlMultiContext::OnClose(uv_handle_t* handle) {
  CurlMultiContext* ctx = ContainerOf(&CurlMultiContext::poll_,
                                      reinterpret_cast<uv_poll_t*>(handle));
  delete ctx;
}

CurlMultiContext::CurlMultiContext(CurlMulti* multi, curl_socket_t sockfd)
    : multi_(multi), sockfd_(sockfd) {
  CHECK_EQ(uv_poll_init_socket(multi_->loop(), &poll_, sockfd_), 0);
}

}  // namespace curl
}  // namespace aworker
