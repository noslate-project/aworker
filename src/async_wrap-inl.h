#ifndef SRC_ASYNC_WRAP_INL_H_
#define SRC_ASYNC_WRAP_INL_H_

#include "async_wrap.h"

namespace aworker {

inline v8::MaybeLocal<v8::Value> AsyncWrap::MakeCallback(
    const v8::Local<v8::String> symbol, int argc, v8::Local<v8::Value>* argv) {
  return MakeCallback(symbol.As<v8::Name>(), argc, argv);
}

inline v8::MaybeLocal<v8::Value> AsyncWrap::MakeCallback(
    const v8::Local<v8::Symbol> symbol, int argc, v8::Local<v8::Value>* argv) {
  return MakeCallback(symbol.As<v8::Name>(), argc, argv);
}

inline v8::MaybeLocal<v8::Value> AsyncWrap::MakeCallback(
    const v8::Local<v8::Name> symbol, int argc, v8::Local<v8::Value>* argv) {
  v8::Local<v8::Value> cb_v;
  if (!object()->Get(immortal()->context(), symbol).ToLocal(&cb_v))
    return v8::MaybeLocal<v8::Value>();
  if (!cb_v->IsFunction()) {
    v8::Isolate* isolate = immortal()->isolate();
    return Undefined(isolate);
  }
  return MakeCallback(cb_v.As<v8::Function>(), argc, argv);
}

}  // namespace aworker

#endif  // SRC_ASYNC_WRAP_INL_H_
