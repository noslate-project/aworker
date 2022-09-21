#include <time.h>  // tzset(), _tzset()
#include <unistd.h>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#include "error_handling.h"
#include "immortal.h"
#include "process.h"
#include "util.h"
#include "utils/async_primitives.h"

namespace aworker {
using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::DontDelete;
using v8::DontEnum;
using v8::EscapableHandleScope;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Name;
using v8::NamedPropertyHandlerConfiguration;
using v8::NewStringType;
using v8::Nothing;
using v8::Object;
using v8::ObjectTemplate;
using v8::PropertyCallbackInfo;
using v8::PropertyHandlerFlags;
using v8::ReadOnly;
using v8::String;
using v8::Value;

template <typename T>
void DateTimeConfigurationChangeNotification(Isolate* isolate, const T& key) {
  if (key.length() == 2 && (*key)[0] == 'T' && (*key)[1] == 'Z') {
#ifdef __POSIX__
    tzset();
#else
    _tzset();
#endif
    auto constexpr time_zone_detection = Isolate::TimeZoneDetection::kRedetect;
    isolate->DateTimeConfigurationChangeNotification(time_zone_detection);
  }
}

Maybe<std::string> ProcessEnvStore::Get(const char* key) const {
  auto value = std::getenv(key);
  if (value != nullptr) {  // Env key value fetch success.
    return Just(std::string(value));
  }

  return Nothing<std::string>();
}

MaybeLocal<String> ProcessEnvStore::Get(Isolate* isolate,
                                        Local<String> property) const {
  String::Utf8Value key(isolate, property);
  Maybe<std::string> value = Get(*key);

  if (value.IsJust()) {
    std::string val = value.FromJust();
    return String::NewFromUtf8(
        isolate, val.data(), NewStringType::kNormal, val.size());
  }

  return MaybeLocal<String>();
}

void ProcessEnvStore::Set(Isolate* isolate,
                          Local<String> property,
                          Local<String> value) {
  String::Utf8Value key(isolate, property);
  String::Utf8Value val(isolate, value);

#ifdef _WIN32
  if (key.length() > 0 && key[0] == '=') return;
#endif
  setenv(*key, *val, 1);
  DateTimeConfigurationChangeNotification(isolate, key);
}

int32_t ProcessEnvStore::Query(const char* key) const {
  auto val = std::getenv(key);
  if (val == nullptr) {
    return -1;
  }

#ifdef _WIN32
  if (key[0] == '=') {
    return static_cast<int32_t>(ReadOnly) | static_cast<int32_t>(DontDelete) |
           static_cast<int32_t>(DontEnum);
  }
#endif

  return 0;
}

int32_t ProcessEnvStore::Query(Isolate* isolate, Local<String> property) const {
  String::Utf8Value key(isolate, property);
  return Query(*key);
}

void ProcessEnvStore::Delete(Isolate* isolate, Local<String> property) {
  String::Utf8Value key(isolate, property);
  unsetenv(*key);
  DateTimeConfigurationChangeNotification(isolate, key);
}

Local<Array> ProcessEnvStore::Enumerate(Isolate* isolate) const {
  env_item_t* items;
  int count;

  auto cleanup = OnScopeLeave([&]() { os_free_environ(items, count); });
  CHECK_EQ(os_environ(&items, &count), 0);

  std::vector<Local<Value>> env_v(count);
  int env_v_index = 0;
  for (int i = 0; i < count; i++) {
#ifdef _WIN32
    // If the key starts with '=' it is a hidden environment variable.
    // The '\0' check is a workaround for the bug behind
    // https://github.com/libuv/libuv/pull/2473 and can be removed later.
    if (items[i].name[0] == '=' || items[i].name[0] == '\0') continue;
#endif
    MaybeLocal<String> str = String::NewFromUtf8(isolate, items[i].name);
    if (str.IsEmpty()) {
      isolate->ThrowException(ERR_STRING_TOO_LONG(isolate));
      // TODO(chengzhong.wcz): safe Local;
      return Local<Array>();
    }
    env_v[env_v_index++] = str.ToLocalChecked();
  }

  return Array::New(isolate, env_v.data(), env_v_index);
}

static void EnvGetter(Local<Name> property,
                      const PropertyCallbackInfo<Value>& info) {
  Immortal* immortal = Immortal::GetCurrent(info);
  if (property->IsSymbol()) {
    return info.GetReturnValue().SetUndefined();
  }
  CHECK(property->IsString());
  MaybeLocal<String> value_string =
      immortal->env_vars()->Get(immortal->isolate(), property.As<String>());
  if (!value_string.IsEmpty()) {
    info.GetReturnValue().Set(value_string.ToLocalChecked());
  }
}

static void EnvSetter(Local<Name> property,
                      Local<Value> value,
                      const PropertyCallbackInfo<Value>& info) {
  Immortal* immortal = Immortal::GetCurrent(info);
  if (!value->IsString() && !value->IsNumber() && !value->IsBoolean()) {
    Local<Value> error = v8::Exception::TypeError(OneByteString(
        immortal->isolate(), "Expects a string, number, or boolean"));
    info.GetIsolate()->ThrowException(error);
    return;
  }

  Local<String> key;
  Local<String> value_string;
  if (!property->ToString(immortal->context()).ToLocal(&key) ||
      !value->ToString(immortal->context()).ToLocal(&value_string)) {
    return;
  }

  immortal->env_vars()->Set(immortal->isolate(), key, value_string);

  // Whether it worked or not, always return value.
  info.GetReturnValue().Set(value);
}

static void EnvQuery(Local<Name> property,
                     const PropertyCallbackInfo<Integer>& info) {
  Immortal* immortal = Immortal::GetCurrent(info);
  if (property->IsString()) {
    int32_t rc =
        immortal->env_vars()->Query(immortal->isolate(), property.As<String>());
    if (rc != -1) info.GetReturnValue().Set(rc);
  }
}

static void EnvDeleter(Local<Name> property,
                       const PropertyCallbackInfo<Boolean>& info) {
  Immortal* immortal = Immortal::GetCurrent(info);
  if (property->IsString()) {
    immortal->env_vars()->Delete(immortal->isolate(), property.As<String>());
  }

  // process.env never has non-configurable properties, so always
  // return true like the tc39 delete operator.
  info.GetReturnValue().Set(true);
}

static void EnvEnumerator(const PropertyCallbackInfo<Array>& info) {
  Immortal* immortal = Immortal::GetCurrent(info);
  info.GetReturnValue().Set(
      immortal->env_vars()->Enumerate(immortal->isolate()));
}

MaybeLocal<Object> CreateEnvVarProxy(Local<Context> context, Isolate* isolate) {
  EscapableHandleScope scope(isolate);
  Local<ObjectTemplate> env_proxy_template = ObjectTemplate::New(isolate);
  // TODO(chengzhong.wcz): safe Local;
  env_proxy_template->SetHandler(NamedPropertyHandlerConfiguration(
      EnvGetter,
      EnvSetter,
      EnvQuery,
      EnvDeleter,
      EnvEnumerator,
      Local<Value>(),
      PropertyHandlerFlags::kHasNoSideEffect));
  return scope.EscapeMaybe(env_proxy_template->NewInstance(context));
}

void ProcessEnvStore::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(EnvGetter);
  registry->Register(EnvSetter);
  registry->Register(EnvQuery);
  registry->Register(EnvDeleter);
  registry->Register(EnvEnumerator);
}
}  // namespace aworker
