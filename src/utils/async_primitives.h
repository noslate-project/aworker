#pragma once
#include <fcntl.h>
#include <unistd.h>
#include <functional>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include "util.h"
#include "uv.h"

namespace aworker {

using ScopedLock = std::lock_guard<std::mutex>;
using UniqueLock = std::unique_lock<std::mutex>;

template <typename Fn>
class UvAsync {
  inline static void Dispose(UvAsync* item) {
    uv_close(reinterpret_cast<uv_handle_t*>(&item->async_),
             [](uv_handle_t* handle) {
               UvAsync* wrap = ContainerOf(
                   &UvAsync::async_, reinterpret_cast<uv_async_t*>(handle));
               delete wrap;
             });
  }

 public:
  using Ptr = DeleteFnPtr<UvAsync, Dispose>;
  inline static Ptr Create(uv_loop_t* loop, Fn&& callback) {
    return Ptr(new UvAsync(loop, std::move(callback)));
  }

  inline void Send() { uv_async_send(&async_); }
  inline void Unref() { uv_unref(reinterpret_cast<uv_handle_t*>(&async_)); }
  inline void Ref() { uv_ref(reinterpret_cast<uv_handle_t*>(&async_)); }

 private:
  inline UvAsync(uv_loop_t* loop, Fn&& callback)
      : loop_(loop), callback_(std::move(callback)) {
    uv_async_init(loop, &async_, AsyncCb);
  }

  inline static void AsyncCb(uv_async_t* handle) {
    UvAsync* wrap = ContainerOf(&UvAsync::async_, handle);
    wrap->callback_();
  }

  uv_loop_t* loop_;
  uv_async_t async_;
  Fn callback_;
};
}  // namespace aworker
