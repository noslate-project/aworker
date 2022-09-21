#include "binding/curl/curl_def.h"
#include "binding/curl/curl_easy.h"
#include "binding/curl/curl_multi.h"
#include "binding/curl/curl_version.h"
#include "immortal.h"

namespace aworker {
namespace curl {

using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Uint32;

AWORKER_METHOD(CurlInit) {
  CHECK_EQ(curl_global_init(CURL_GLOBAL_ALL), 0);
}

AWORKER_METHOD(EasyStrErr) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  CURLcode code = static_cast<CURLcode>(info[0].As<v8::Int32>()->Value());
  Local<String> str = v8::String::NewFromUtf8(isolate, curl_easy_strerror(code))
                          .ToLocalChecked();
  info.GetReturnValue().Set(str);
}

AWORKER_BINDING(Init) {
  Isolate* isolate = immortal->isolate();
  CurlVersions::Initialize(exports, context, immortal);
  CurlMulti::Initialize(exports, context, immortal);
  CurlEasy::Initialize(exports, context, immortal);

  immortal->SetFunctionProperty(exports, "init", CurlInit);
  immortal->SetFunctionProperty(exports, "easyStrErr", EasyStrErr);

  Local<Object> curlopt = Object::New(isolate);
#define V(VAR)                                                                 \
  immortal->SetValueProperty(curlopt, #VAR, Uint32::New(isolate, VAR));
  CURL_OPTS_INTEGER(V)
  CURL_OPTS_STRING(V)
  CURL_OPTS_LIST(V)
#undef V
  immortal->SetValueProperty(exports, "Options", curlopt);

  Local<Object> curl_code = Object::New(isolate);
#define V(VAR)                                                                 \
  immortal->SetValueProperty(curl_code, #VAR, Uint32::New(isolate, VAR));
  CURL_CODE_ALL(V)
#undef V
  immortal->SetValueProperty(exports, "Codes", curl_code);

  Local<Object> curl_ipresolve = Object::New(isolate);
#define V(VAR)                                                                 \
  immortal->SetValueProperty(curl_ipresolve, #VAR, Uint32::New(isolate, VAR));
  CURL_IPRESOLVE_OPTS(V)
#undef V
  immortal->SetValueProperty(exports, "IpResolveOptions", curl_ipresolve);

  Local<Object> curl_ssl = Object::New(isolate);
#define V(VAR)                                                                 \
  immortal->SetValueProperty(curl_ssl, #VAR, Uint32::New(isolate, VAR));
  CURL_SSL_OPTS(V)
#undef V
  immortal->SetValueProperty(exports, "SslOptions", curl_ssl);

#define V(VAR)                                                                 \
  immortal->SetValueProperty(exports, #VAR, Uint32::New(isolate, VAR));
  CURL_PAUSE_OPTS(V)
  CURL_READFUNC_FLAGS(V)
#undef V
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  registry->Register(CurlInit);
  registry->Register(EasyStrErr);
  CurlVersions::Initialize(registry);
  CurlMulti::Initialize(registry);
  CurlEasy::Initialize(registry);
}

}  // namespace curl
}  // namespace aworker

AWORKER_BINDING_REGISTER(curl, aworker::curl::Init, aworker::curl::Init)
