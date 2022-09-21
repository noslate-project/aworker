#include "url/url.h"
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
struct ToV8Traits<Url> {
  static v8::MaybeLocal<v8::Value> ToV8(v8::Local<v8::Context> context,
                                        const Url& url) {
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
    V(origin, Origin)
    V(protocol, Protocol)
    V(username, Username)
    V(password, Password)
    V(host, Host)
    V(hostname, Hostname)
    V(port, Port)
    V(pathname, Pathname)
    V(search, Search)
    V(hash, Hash)
    V(href, Href)
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
  Utf8Value url_utf8(isolate, info[0].As<String>());

  Url url;
  if (info[1]->IsString()) {
    Utf8Value base_utf8(isolate, info[1].As<String>());
    url = Url(url_utf8.ToString(), base_utf8.ToString());
  } else {
    url = Url(url_utf8.ToString());
  }
  if (!url.is_valid()) {
    ThrowException(isolate, "Invalid URL", ExceptionType::kTypeError);
    return;
  }

  MaybeLocal<Value> maybe_url = ToV8Traits<Url>::ToV8(immortal->context(), url);
  if (maybe_url.IsEmpty()) {
    return;
  }
  info.GetReturnValue().Set(maybe_url.ToLocalChecked());
}

template <std::string (*func)(const std::string&)>
AWORKER_METHOD(UpdateUrl) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  CHECK(info[0]->IsString());
  Utf8Value val_utf8(isolate, info[0].As<String>());

  std::string result = func(val_utf8.ToString());
  MaybeLocal<Value> maybe_result =
      ToV8Traits<std::string>::ToV8(immortal->context(), result);
  if (maybe_result.IsEmpty()) {
    return;
  }
  info.GetReturnValue().Set(maybe_result.ToLocalChecked());
}

template <std::string (*func)(const std::string&, const std::string&)>
AWORKER_METHOD(UpdateUrl) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  CHECK(info[0]->IsString());
  CHECK(info[1]->IsString());
  Utf8Value protocol_utf8(isolate, info[0].As<String>());
  Utf8Value val_utf8(isolate, info[1].As<String>());

  std::string result = func(protocol_utf8.ToString(), val_utf8.ToString());
  MaybeLocal<Value> maybe_result =
      ToV8Traits<std::string>::ToV8(immortal->context(), result);
  if (maybe_result.IsEmpty()) {
    return;
  }
  info.GetReturnValue().Set(maybe_result.ToLocalChecked());
}

AWORKER_METHOD(UpdateHost) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal->context();

  CHECK(info[0]->IsString());
  CHECK(info[1]->IsString());
  Utf8Value protocol_utf8(isolate, info[0].As<String>());
  Utf8Value val_utf8(isolate, info[1].As<String>());

  std::pair<std::string, std::string> result =
      Url::UpdateHost(protocol_utf8.ToString(), val_utf8.ToString());
  MaybeLocal<Value> maybe_result =
      ToV8Traits<std::pair<std::string, std::string>>::ToV8(context, result);
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
  V(updateProtocol, Url::UpdateProtocol)
  V(updateUserInfo, Url::UpdateUserInfo)
  V(updatePort, Url::UpdatePort)
  V(updateHash, Url::UpdateHash)
  V(updateHostname, Url::UpdateHostname)
  V(updatePathname, Url::UpdatePathname)
  V(updateSearch, Url::UpdateSearch)
#undef V
  immortal->SetFunctionProperty(exports, "updateHost", UpdateHost);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  registry->Register(ParseUrl);
  registry->Register(ParseSearchParams);
  registry->Register(SerializeSearchParams);

#define V(func) registry->Register(UpdateUrl<func>);
  V(Url::UpdateProtocol)
  V(Url::UpdateUserInfo)
  V(Url::UpdatePort)
  V(Url::UpdateHash)
  V(Url::UpdateHostname)
  V(Url::UpdatePathname)
  V(Url::UpdateSearch)
#undef V
  registry->Register(UpdateHost);
}

}  // namespace url
}  // namespace aworker

AWORKER_BINDING_REGISTER(url, aworker::url::Init, aworker::url::Init)
