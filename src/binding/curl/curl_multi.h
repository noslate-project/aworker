#ifndef SRC_BINDING_CURL_CURL_MULTI_H_
#define SRC_BINDING_CURL_CURL_MULTI_H_

#include <curl/curl.h>
#include <memory>
#include <set>

#include "async_wrap.h"
#include "aworker_binding.h"

namespace aworker {
namespace curl {

class CurlMultiContext;
class CurlEasy;

// https://curl.se/libcurl/c/multi-uv.html
class CurlMulti : public AsyncWrap {
  DEFINE_WRAPPERTYPEINFO();
  AWORKER_DISALLOW_ASSIGN_COPY(CurlMulti);

 public:
  static AWORKER_BINDING(Initialize);
  static AWORKER_EXTERNAL_REFERENCE(Initialize);

  static AWORKER_METHOD(New);
  static AWORKER_METHOD(SetOpt);
  static AWORKER_METHOD(AddHandle);
  static AWORKER_METHOD(RemoveHandle);
  static AWORKER_METHOD(Close);

  uv_loop_t* loop() { return loop_; }

  void OnPoll(CurlMultiContext* ctx, int flags);

 private:
  static int HandleSocket(
      CURL* easy, curl_socket_t socket, int action, void* userp, void* socketp);
  static int StartTimeout(CURLM* multi, int64_t timeout_ms, void* userp);
  static void OnTimeout(uv_timer_t* req);
  static void OnClose(uv_handle_t* handle);

  void ProcessMessages();

  CurlMulti(Immortal* immortal, v8::Local<v8::Object> object);
  ~CurlMulti();

  uv_loop_t* loop_;
  uv_timer_t timer_;
  CURLM* multi_handle_;
  std::set<CurlEasy*> pending_handles_;
};

class CurlMultiContext {
  AWORKER_DISALLOW_ASSIGN_COPY(CurlMultiContext);

 public:
  static CurlMultiContext* Create(CurlMulti* multi, curl_socket_t sockfd);
  void Destroy();

  void Start(int events);

  curl_socket_t sockfd() { return sockfd_; }

 private:
  static void OnPoll(uv_poll_t* req, int status, int events);
  static void OnClose(uv_handle_t* handle);
  CurlMultiContext(CurlMulti* multi, curl_socket_t sockfd);

  CurlMulti* multi_;
  curl_socket_t sockfd_;

  uv_poll_t poll_;
};

}  // namespace curl
}  // namespace aworker

#endif  // SRC_BINDING_CURL_CURL_MULTI_H_
