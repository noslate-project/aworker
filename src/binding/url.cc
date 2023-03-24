#include "url/url.h"
#include "ada.h"
#include "aworker_binding.h"
#include "binding/core/to_v8_traits.h"
#include "error_handling.h"

namespace aworker {
using std::string;

using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Name;
using v8::Object;
using v8::ObjectTemplate;
using v8::PropertyCallbackInfo;
using v8::String;
using v8::Symbol;
using v8::Value;

template <>
struct ToV8Traits<ada::url> {
  static v8::MaybeLocal<v8::Value> ToV8(v8::Local<v8::Context> context,
                                        const ada::url& url) {
    Isolate* isolate = context->GetIsolate();
    Local<v8::Object> obj = v8::Object::New(isolate);
#define V(key, Key)                                                            \
  {                                                                            \
    v8::MaybeLocal<v8::Value> value =                                          \
        ToV8Traits<std::string>::ToV8(context, url.Key());                     \
    if (value.IsEmpty()) {                                                     \
      return value;                                                            \
    }                                                                          \
    obj->Set(context, OneByteString(isolate, #key), value.ToLocalChecked())    \
        .ToChecked();                                                          \
  }
    V(origin, get_origin)
    V(protocol, get_protocol)
    V(username, get_username)
    V(password, get_password)
    V(host, get_host)
    V(hostname, get_hostname)
    V(port, get_port)
    V(pathname, get_pathname)
    V(search, get_search)
    V(hash, get_hash)
    V(href, get_href)
#undef V

    return obj;
  }
};

namespace url {

AWORKER_METHOD(ParseSearchParams) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  CHECK(info[0]->IsString());
  Utf8Value search_params_utf8(isolate, info[0].As<String>());

  UrlSearchParams search_params(search_params_utf8.ToString());
  MaybeLocal<Value> maybe_params =
      ToV8Traits<std::vector<UrlSearchParams::ParamEntry>>::ToV8(
          immortal->context(), search_params.params());
  if (maybe_params.IsEmpty()) {
    return;
  }
  info.GetReturnValue().Set(maybe_params.ToLocalChecked());
}

AWORKER_METHOD(SerializeSearchParams) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal->context();

  CHECK(info[0]->IsArray());
  Local<Array> js_arr = info[0].As<Array>();
  std::vector<UrlSearchParams::ParamEntry> list(js_arr->Length());
  for (uint32_t idx = 0; idx < js_arr->Length(); idx++) {
    Local<Array> tuple = js_arr->Get(context, idx).ToLocalChecked().As<Array>();
    Utf8Value key(isolate,
                  tuple->Get(context, 0).ToLocalChecked().As<String>());
    Utf8Value value(isolate,
                    tuple->Get(context, 1).ToLocalChecked().As<String>());
    list[idx] = std::make_pair(key.ToString(), value.ToString());
  }
  UrlSearchParams search_params(list);
  Local<String> utf8_string = Utf8String(isolate, search_params.ToString());
  info.GetReturnValue().Set(utf8_string);
}

AWORKER_METHOD(ParseUrl) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  CHECK(info[0]->IsString());
  Utf8Value url_utf8(isolate, info[0]);

  ada::result base;
  ada::url* base_pointer = nullptr;
  if (info[1]->IsString()) {
    base = ada::parse(Utf8Value(isolate, info[1]).ToString());
    if (!base) {
      ThrowException(isolate, "Invalid base URL", ExceptionType::kTypeError);
      return;
    }
    base_pointer = &base.value();
  }

  ada::result out = ada::parse(url_utf8.ToString(), base_pointer);
  if (!out) {
    ThrowException(isolate, "Invalid URL", ExceptionType::kTypeError);
    return;
  }

  MaybeLocal<Value> maybe_url =
      ToV8Traits<ada::url>::ToV8(immortal->context(), out.value());
  if (maybe_url.IsEmpty()) {
    return;
  }
  info.GetReturnValue().Set(maybe_url.ToLocalChecked());
}

template <void (ada::url::*func)(const std::string_view)>
AWORKER_METHOD(UpdateUrl) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  CHECK(info[0]->IsString());
  CHECK(info[1]->IsString());
  Utf8Value href_utf8(isolate, info[0]);
  Utf8Value val_utf8(isolate, info[1]);

  ada::result result = ada::parse(href_utf8.ToString());
  if (!result) {
    ThrowException(isolate, "Invalid URL", ExceptionType::kTypeError);
    return;
  }

  (result.value().*func)(val_utf8.ToString());

  MaybeLocal<Value> maybe_result =
      ToV8Traits<ada::url>::ToV8(immortal->context(), result.value());
  if (maybe_result.IsEmpty()) {
    return;
  }
  info.GetReturnValue().Set(maybe_result.ToLocalChecked());
}

template <bool (ada::url::*func)(const std::string_view)>
AWORKER_METHOD(UpdateUrl) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  CHECK(info[0]->IsString());
  CHECK(info[1]->IsString());
  Utf8Value href_utf8(isolate, info[0]);
  Utf8Value val_utf8(isolate, info[1]);

  ada::result result = ada::parse(href_utf8.ToString());
  if (!result) {
    ThrowException(isolate, "Invalid URL", ExceptionType::kTypeError);
    return;
  }

  bool success = (result.value().*func)(val_utf8.ToString());
  if (!success) {
    ThrowException(isolate, "Invalid URL update", ExceptionType::kTypeError);
    return;
  }

  MaybeLocal<Value> maybe_result =
      ToV8Traits<ada::url>::ToV8(immortal->context(), result.value());
  if (maybe_result.IsEmpty()) {
    return;
  }
  info.GetReturnValue().Set(maybe_result.ToLocalChecked());
}

AWORKER_BINDING(Init) {
  immortal->SetFunctionProperty(
      exports, "parseSearchParams", ParseSearchParams);
  immortal->SetFunctionProperty(
      exports, "serializeSearchParams", SerializeSearchParams);

  immortal->SetFunctionProperty(exports, "parseUrl", ParseUrl);
#define V(name, func)                                                          \
  immortal->SetFunctionProperty(exports, #name, UpdateUrl<func>);
  V(updateProtocol, &ada::url::set_protocol)
  V(updateUsername, &ada::url::set_username)
  V(updatePassword, &ada::url::set_password)
  V(updatePort, &ada::url::set_port)
  V(updateHash, &ada::url::set_hash)
  V(updateHost, &ada::url::set_host)
  V(updateHostname, &ada::url::set_hostname)
  V(updateHref, &ada::url::set_href)
  V(updatePathname, &ada::url::set_pathname)
  V(updateSearch, &ada::url::set_search)
#undef V
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  registry->Register(ParseSearchParams);
  registry->Register(SerializeSearchParams);

  registry->Register(ParseUrl);
#define V(func) registry->Register(UpdateUrl<func>);
  V(&ada::url::set_protocol)
  V(&ada::url::set_username)
  V(&ada::url::set_password)
  V(&ada::url::set_port)
  V(&ada::url::set_hash)
  V(&ada::url::set_host)
  V(&ada::url::set_hostname)
  V(&ada::url::set_href)
  V(&ada::url::set_pathname)
  V(&ada::url::set_search)
#undef V
}

}  // namespace url
}  // namespace aworker

AWORKER_BINDING_REGISTER(url, aworker::url::Init, aworker::url::Init)
