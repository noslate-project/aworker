#ifndef SRC_BINDING_CURL_CURL_EASY_H_
#define SRC_BINDING_CURL_CURL_EASY_H_

#include <curl/curl.h>

#include "async_wrap.h"

namespace aworker {
namespace curl {

class CurlEasy : public AsyncWrap {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AWORKER_BINDING(Initialize);
  static AWORKER_EXTERNAL_REFERENCE(Initialize);

  static AWORKER_METHOD(New);
  static AWORKER_METHOD(SetOpt);
  static AWORKER_METHOD(Pause);

  static CurlEasy* FromEasyHandle(CURL* easy_handle);

  CURL* easy_handle() { return easy_handle_; }
  void OnDone(CURLcode code);

 private:
  static size_t OnHeader(char* buffer,
                         size_t size,
                         size_t nitems,
                         void* userdata);
  static size_t OnWrite(char* ptr, size_t size, size_t nmemb, void* userdata);
  static size_t OnRead(char* buffer,
                       size_t size,
                       size_t nitems,
                       void* userdata);
  static int OnPreReq(void* clientp,
                      char* conn_primary_ip,
                      char* conn_local_ip,
                      int conn_primary_port,
                      int conn_local_port);

  CurlEasy(Immortal* immortal, v8::Local<v8::Object> object);
  ~CurlEasy();

  CURL* easy_handle_;
  struct curl_slist* headers_;
};

}  // namespace curl
}  // namespace aworker

#endif  // SRC_BINDING_CURL_CURL_EASY_H_
