#include "binding/curl/curl_easy.h"
#include "error_handling.h"
#include "utils/resizable_buffer.h"

namespace aworker {
namespace curl {
using v8::Array;
using v8::ArrayBuffer;
using v8::BackingStore;
using v8::Context;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;
using v8::Uint32;
using v8::Value;

static char* root_certs = {
#include "binding/curl/root_certs.h"  // NOLINT(build/include_order)
};

const WrapperTypeInfo CurlEasy::wrapper_type_info_{
    "curl_easy",
};

AWORKER_BINDING(CurlEasy::Initialize) {
  Isolate* isolate = immortal->isolate();
  Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, CurlEasy::New);
  tpl->Inherit(AsyncWrap::GetConstructorTemplate(immortal));
  tpl->InstanceTemplate()->SetInternalFieldCount(
      BaseObject::kInternalFieldCount);

  Local<String> name = OneByteString(isolate, "CurlEasy");
  tpl->SetClassName(name);

  Local<ObjectTemplate> prototype_template = tpl->PrototypeTemplate();
  immortal->SetFunctionProperty(prototype_template, "setOpt", SetOpt);
  immortal->SetFunctionProperty(prototype_template, "pause", Pause);

  exports->Set(context, name, tpl->GetFunction(context).ToLocalChecked())
      .Check();
}

AWORKER_EXTERNAL_REFERENCE(CurlEasy::Initialize) {
  registry->Register(New);
  registry->Register(SetOpt);
  registry->Register(Pause);
}

AWORKER_METHOD(CurlEasy::New) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  new CurlEasy(immortal, info.This());

  info.GetReturnValue().Set(info.This());
}

AWORKER_METHOD(CurlEasy::SetOpt) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal->context();

  CurlEasy* ce;
  ASSIGN_OR_RETURN_UNWRAP(&ce, info.This());

  CURLoption opt = static_cast<CURLoption>(info[0].As<v8::Uint32>()->Value());
  if (info[1]->IsInt32()) {
    curl_easy_setopt(ce->easy_handle_, opt, info[1].As<v8::Int32>()->Value());
    return;
  } else if (info[1]->IsString()) {
    Utf8Value utf8_value(isolate, info[1]);
    curl_easy_setopt(ce->easy_handle_, opt, *utf8_value);
    return;
  } else if (info[1]->IsArray()) {
    Local<Array> arr = info[1].As<Array>();
    struct curl_slist* slist = nullptr;
    struct curl_slist* temp = nullptr;

    for (uint32_t idx = 0; idx < arr->Length(); idx++) {
      Local<Value> item;
      if (!arr->Get(context, idx).ToLocal(&item)) {
        return;
      }
      if (opt == CURLOPT_HTTPHEADER) {
        Local<String> str = item.As<String>();
        // including the null terminator
        ResizableBuffer buf(str->Length() + 1);
        str->WriteOneByte(isolate, static_cast<uint8_t*>(buf.buffer()));
        temp = curl_slist_append(slist, static_cast<const char*>(buf.buffer()));
      } else {
        Utf8Value utf8_value(isolate, item);
        temp = curl_slist_append(slist, *utf8_value);
      }
      if (temp == nullptr) {
        curl_slist_free_all(slist);
        ThrowException(isolate, "failed to create slist");
        return;
      }
      slist = temp;
    }
    curl_easy_setopt(ce->easy_handle_, opt, slist);

    if (opt == CURLOPT_HTTPHEADER) {
      if (ce->headers_) curl_slist_free_all(ce->headers_);
      ce->headers_ = slist;
    }
    return;
  }
  UNREACHABLE();
}

AWORKER_METHOD(CurlEasy::Pause) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  CurlEasy* ce;
  ASSIGN_OR_RETURN_UNWRAP(&ce, info.This());

  int bitmask = info[0].As<v8::Int32>()->Value();
  CURLcode code = curl_easy_pause(ce->easy_handle_, bitmask);
  info.GetReturnValue().Set(static_cast<int32_t>(code));
}

// static
CurlEasy* CurlEasy::FromEasyHandle(CURL* easy_handle) {
  // From https://curl.haxx.se/libcurl/c/CURLINFO_PRIVATE.html
  // > Please note that for internal reasons, the value is returned as a char
  // pointer, although effectively being a 'void *'.
  CurlEasy* ptr = nullptr;
  CURLcode code = curl_easy_getinfo(
      easy_handle, CURLINFO_PRIVATE, reinterpret_cast<char*>(&ptr));
  if (code != CURLE_OK) {
    return nullptr;
  }

  return ptr;
}

CurlEasy::CurlEasy(Immortal* immortal, Local<Object> obj)
    : AsyncWrap(immortal, obj), easy_handle_(nullptr), headers_(nullptr) {
  MakeWeak();

  easy_handle_ = curl_easy_init();

  // Refer to https://curl.se/libcurl/c/curl_easy_setopt.html for more info.
  curl_easy_setopt(easy_handle_, CURLOPT_PRIVATE, this);

  struct curl_blob blob;
  blob.data = root_certs;
  blob.len = strlen(root_certs);
  blob.flags = CURL_BLOB_NOCOPY;
  curl_easy_setopt(easy_handle_, CURLOPT_CAINFO_BLOB, &blob);

  curl_easy_setopt(easy_handle_, CURLOPT_HEADERDATA, this);
  curl_easy_setopt(easy_handle_, CURLOPT_HEADERFUNCTION, OnHeader);

  curl_easy_setopt(easy_handle_, CURLOPT_WRITEDATA, this);
  curl_easy_setopt(easy_handle_, CURLOPT_WRITEFUNCTION, OnWrite);

  curl_easy_setopt(easy_handle_, CURLOPT_READDATA, this);
  curl_easy_setopt(easy_handle_, CURLOPT_READFUNCTION, OnRead);

  curl_easy_setopt(easy_handle_, CURLOPT_PREREQDATA, this);
  curl_easy_setopt(easy_handle_, CURLOPT_PREREQFUNCTION, OnPreReq);
}

CurlEasy::~CurlEasy() {
  if (headers_) curl_slist_free_all(headers_);
  curl_easy_cleanup(easy_handle_);
}

void CurlEasy::OnDone(CURLcode code) {
  HandleScope scope(isolate());
  curl_off_t total_time_t;
  curl_off_t namelookup_time_t;
  curl_off_t connect_time_t;
  curl_off_t appconnect_time_t;
  curl_off_t pretransfer_time_t;
  curl_off_t starttransfer_time_t;
  curl_off_t redirect_time_t;

  curl_easy_getinfo(easy_handle_, CURLINFO_TOTAL_TIME_T, &total_time_t);
  curl_easy_getinfo(
      easy_handle_, CURLINFO_NAMELOOKUP_TIME_T, &namelookup_time_t);
  curl_easy_getinfo(easy_handle_, CURLINFO_CONNECT_TIME_T, &connect_time_t);
  curl_easy_getinfo(
      easy_handle_, CURLINFO_APPCONNECT_TIME_T, &appconnect_time_t);
  curl_easy_getinfo(
      easy_handle_, CURLINFO_PRETRANSFER_TIME_T, &pretransfer_time_t);
  curl_easy_getinfo(
      easy_handle_, CURLINFO_STARTTRANSFER_TIME_T, &starttransfer_time_t);
  curl_easy_getinfo(easy_handle_, CURLINFO_REDIRECT_TIME_T, &redirect_time_t);

  Local<v8::Value> argv[] = {v8::Uint32::New(isolate(), code),
                             v8::Integer::New(isolate(), total_time_t),
                             v8::Integer::New(isolate(), namelookup_time_t),
                             v8::Integer::New(isolate(), connect_time_t),
                             v8::Integer::New(isolate(), appconnect_time_t),
                             v8::Integer::New(isolate(), pretransfer_time_t),
                             v8::Integer::New(isolate(), starttransfer_time_t),
                             v8::Integer::New(isolate(), redirect_time_t)};
  MakeCallback(OneByteString(isolate(), "_onDone"), arraysize(argv), argv);
}

// static
size_t CurlEasy::OnHeader(char* buffer,
                          size_t size,
                          size_t nitems,
                          void* userdata) {
  CurlEasy* ce = static_cast<CurlEasy*>(userdata);
  Isolate* isolate = ce->isolate();
  HandleScope scope(isolate);

  MaybeLocal<String> maybe_header_line =
      String::NewFromOneByte(isolate,
                             reinterpret_cast<uint8_t*>(buffer),
                             v8::NewStringType::kNormal,
                             nitems);
  Local<String> header_line;
  if (!maybe_header_line.ToLocal(&header_line)) {
    return 0;
  }
  Local<v8::Value> argv[] = {header_line};
  ce->MakeCallback(OneByteString(isolate, "_onHeader"), arraysize(argv), argv);
  return nitems;
}

// static
size_t CurlEasy::OnWrite(char* ptr, size_t size, size_t nmemb, void* userdata) {
  CurlEasy* ce = static_cast<CurlEasy*>(userdata);
  Isolate* isolate = ce->isolate();
  HandleScope scope(isolate);

  std::shared_ptr<BackingStore> bs =
      ArrayBuffer::NewBackingStore(isolate, nmemb);
  memcpy(bs->Data(), ptr, nmemb);
  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, bs);

  Local<v8::Value> argv[] = {ab};
  MaybeLocal<Value> ret = ce->MakeCallback(
      OneByteString(isolate, "_onWrite"), arraysize(argv), argv);
  if (ret.IsEmpty()) {
    return 0;
  }

  return nmemb;
}

// static
size_t CurlEasy::OnRead(char* buffer,
                        size_t size,
                        size_t nitems,
                        void* userdata) {
  CurlEasy* ce = static_cast<CurlEasy*>(userdata);
  Isolate* isolate = ce->isolate();
  HandleScope scope(isolate);

  // copy as much data as possible into the 'ptr' buffer, but no more than
  // 'size' * 'nmemb' bytes.
  size_t nread = size * nitems;
  std::shared_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
      buffer,
      nread,
      [](void* data, size_t length, void* deleter_data) {},
      nullptr);
  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, bs);

  Local<v8::Value> argv[] = {
      Uint32::New(isolate, static_cast<uint32_t>(nread)),
      ab,
  };
  MaybeLocal<Value> maybe_ret = ce->MakeCallback(
      OneByteString(isolate, "_onRead"), arraysize(argv), argv);

  ab->Detach();

  Local<Value> ret;
  if (!maybe_ret.ToLocal(&ret)) {
    return CURL_READFUNC_ABORT;
  }

  uint32_t retcode;
  if (!ret->Uint32Value(ce->immortal()->context()).To(&retcode)) {
    return CURL_READFUNC_ABORT;
  }

  return static_cast<size_t>(retcode);
}

// static
int CurlEasy::OnPreReq(void* clientp,
                       char* conn_primary_ip,
                       char* conn_local_ip,
                       int conn_primary_port,
                       int conn_local_port) {
  CurlEasy* ce = static_cast<CurlEasy*>(clientp);
  Isolate* isolate = ce->isolate();
  HandleScope scope(isolate);

  Local<Value> conn_primary_ip_str = v8::Undefined(isolate);
  Local<Value> conn_local_ip_str = v8::Undefined(isolate);

  USE(String::NewFromUtf8(isolate, conn_primary_ip, v8::NewStringType::kNormal)
          .ToLocal(&conn_primary_ip_str));
  USE(String::NewFromUtf8(isolate, conn_local_ip, v8::NewStringType::kNormal)
          .ToLocal(&conn_local_ip_str));

  Local<v8::Value> argv[] = {
      conn_primary_ip_str,
      conn_local_ip_str,
      v8::Number::New(isolate, conn_primary_port),
      v8::Number::New(isolate, conn_local_port),
  };
  ce->MakeCallback(OneByteString(isolate, "_onPreReq"), arraysize(argv), argv);

  return CURLE_OK;
}

}  // namespace curl
}  // namespace aworker
