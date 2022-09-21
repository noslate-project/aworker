#pragma once

#include "async_wrap.h"
#include "util.h"
#include "uv.h"
#include "v8.h"

namespace aworker {

class Immortal;

// Rules:
//
// - MakeCallback may only be made directly off the event loop.
//   That is there can be no JavaScript stack frames underneath it.
//   (Is there any way to assert that?)
//
// - No use of v8::WeakReferenceCallback. The close callback signifies that
//   we're done with a handle - external resources can be freed.
//
// - The uv_close_cb is used to free the c++ object. The close callback
//   is not made into javascript land.
//

class HandleWrap : public AsyncWrap {
 public:
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Ref(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Unref(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HasRef(const v8::FunctionCallbackInfo<v8::Value>& args);

  static inline bool IsAlive(const HandleWrap* wrap) {
    return wrap != nullptr && wrap->_state != kClosed;
  }

  static inline bool HasRef(const HandleWrap* wrap) {
    return IsAlive(wrap) && uv_has_ref(wrap->handle());
  }

  inline uv_handle_t* handle() const { return _handle; }
  template <typename T>
  inline T* GetHandle() const {
    return reinterpret_cast<T*>(_handle);
  }

  virtual void Close(
      v8::Local<v8::Value> close_callback = v8::Local<v8::Value>());

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Immortal* immortal);

 protected:
  HandleWrap(Immortal* immortal,
             v8::Local<v8::Object> object,
             uv_handle_t* handle);
  virtual void OnClose() {}
  void OnGCCollect() final;

  inline bool IsHandleClosing() const {
    return _state == kClosing || _state == kClosed;
  }

  static void OnClose(uv_handle_t* handle);

 private:
  enum { kInitialized, kClosing, kClosed } _state;
  uv_handle_t* const _handle;
};
}  // namespace aworker
