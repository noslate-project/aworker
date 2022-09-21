#pragma once

#include <list>
#include <string>

#include "aworker_binding.h"
#include "base_object.h"
#include "immortal.h"
#include "util.h"
#include "v8.h"

namespace aworker {

class CallbackScope {
 public:
  explicit CallbackScope(Immortal* immortal, v8::Local<v8::Object> resource);
  ~CallbackScope();

  v8::Local<v8::Object> resource() { return resource_; }

 private:
  Immortal* immortal_;
  CallbackScope* bottom_;
  v8::Local<v8::Object> resource_;
};

class AsyncWrap : public BaseObject {
 public:
  explicit AsyncWrap(Immortal* immortal, v8::Local<v8::Object> handle);
  virtual ~AsyncWrap() = default;

  // Receiver must be the object() to prevent from leaking to arbitrary targets.
  [[deprecated(
      "Use MakeCallback(v8::Local<v8::Function>,int,v8::Local<v8::Value>*) "
      "instead")]] v8::MaybeLocal<v8::Value>
  MakeCallback(v8::Local<v8::Function> cb,
               v8::Local<v8::Value> recv,
               int argc,
               v8::Local<v8::Value>* argv);

  v8::MaybeLocal<v8::Value> MakeCallback(v8::Local<v8::Function> cb,
                                         int argc,
                                         v8::Local<v8::Value>* argv);

  inline v8::MaybeLocal<v8::Value> MakeCallback(
      const v8::Local<v8::Symbol> symbol, int argc, v8::Local<v8::Value>* argv);
  inline v8::MaybeLocal<v8::Value> MakeCallback(
      const v8::Local<v8::String> symbol, int argc, v8::Local<v8::Value>* argv);
  inline v8::MaybeLocal<v8::Value> MakeCallback(
      const v8::Local<v8::Name> symbol, int argc, v8::Local<v8::Value>* argv);

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Immortal* immortal);

  static void AddContext(v8::Local<v8::Context> context);
  static AWORKER_METHOD(SetJSPromiseHooks);

 private:
  static void EmitAsyncInit(Immortal* immortal,
                            v8::Local<v8::Object> resource,
                            v8::Local<v8::Value> trigger_resource);
  static void EmitAsyncBefore(Immortal* immortal,
                              v8::Local<v8::Object> resource);
  static void EmitAsyncAfter(Immortal* immortal,
                             v8::Local<v8::Object> resource);

  friend CallbackScope;
};

}  // namespace aworker

#include "async_wrap-inl.h"
