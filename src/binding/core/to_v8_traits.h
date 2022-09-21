#ifndef SRC_BINDING_CORE_TO_V8_TRAITS_H_
#define SRC_BINDING_CORE_TO_V8_TRAITS_H_

#include <string>
#include "v8.h"

namespace aworker {

// ToV8Traits provides C++ -> V8 conversion.
// Primary template for ToV8Traits.
template <typename T>
struct ToV8Traits;

template <typename R, typename T>
v8::MaybeLocal<R> MaybeLocalCast(v8::MaybeLocal<T> val) {
  if (val.IsEmpty()) {
    return {};
  }
  return val.ToLocalChecked();
}

// std::string
template <>
struct ToV8Traits<std::string> {
  static v8::MaybeLocal<v8::Value> ToV8(v8::Local<v8::Context> context,
                                        const std::string& script_value) {
    return MaybeLocalCast<v8::Value>(
        v8::String::NewFromUtf8(context->GetIsolate(),
                                script_value.c_str(),
                                v8::NewStringType::kNormal,
                                script_value.length()));
  }
};

// std::vector
template <typename T>
struct ToV8Traits<std::vector<T>> {
  static v8::MaybeLocal<v8::Value> ToV8(v8::Local<v8::Context> context,
                                        const std::vector<T>& script_value) {
    v8::Local<v8::Array> arr =
        v8::Array::New(context->GetIsolate(), script_value.size());
    uint32_t idx = 0;
    for (auto& it : script_value) {
      v8::MaybeLocal<v8::Value> val = ToV8Traits<T>::ToV8(context, it);
      if (val.IsEmpty()) {
        return {};
      }
      arr->Set(context, idx, val.ToLocalChecked()).ToChecked();
      idx++;
    }
    return arr;
  }
};

// std::pair
template <typename T, typename R>
struct ToV8Traits<std::pair<T, R>> {
  static v8::MaybeLocal<v8::Value> ToV8(v8::Local<v8::Context> context,
                                        const std::pair<T, R>& script_value) {
    v8::Local<v8::Array> arr = v8::Array::New(context->GetIsolate(), 2);
    v8::MaybeLocal<v8::Value> val =
        ToV8Traits<T>::ToV8(context, script_value.first);
    if (val.IsEmpty()) {
      return {};
    }
    arr->Set(context, 0, val.ToLocalChecked()).ToChecked();

    val = ToV8Traits<T>::ToV8(context, script_value.second);
    if (val.IsEmpty()) {
      return {};
    }
    arr->Set(context, 1, val.ToLocalChecked()).ToChecked();

    return arr;
  }
};

}  // namespace aworker

#endif  // SRC_BINDING_CORE_TO_V8_TRAITS_H_
