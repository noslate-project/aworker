#include "ipc/uv_loop.h"
#include "util.h"

namespace aworker {
namespace ipc {

class UvIdle : public Immediate {
 public:
  explicit UvIdle(Immediate::Callback callback) : Immediate(callback) {}

 private:
  static void IdleCb(uv_idle_t* handle);
  static void CloseCb(uv_handle_t* handle);
  uv_idle_t handle_;
  friend UvLoop;
};

class UvTimer : public Timer {
 public:
  explicit UvTimer(Callback callback) : Timer(callback) {}

 private:
  static void TimerCb(uv_timer_t* handle);
  static void CloseCb(uv_handle_t* handle);
  uv_timer_t handle_;
  friend UvLoop;
};

Immediate* UvLoop::SetImmediate(Immediate::Callback callback) {
  UvIdle* idle = new UvIdle(callback);

  CHECK_EQ(uv_idle_init(*this, &idle->handle_), 0);
  CHECK_EQ(uv_idle_start(&idle->handle_, UvIdle::IdleCb), 0);

  return idle;
}

void UvLoop::ClearImmediate(Immediate* immediate) {
  UvIdle* idle_ = static_cast<UvIdle*>(immediate);
  uv_close(reinterpret_cast<uv_handle_t*>(&idle_->handle_), UvIdle::CloseCb);
}

Timer* UvLoop::SetTimeout(Timer::Callback callback, uint64_t timeout) {
  UvTimer* timer = new UvTimer(callback);

  CHECK_EQ(uv_timer_init(*this, &timer->handle_), 0);
  CHECK_EQ(uv_timer_start(&timer->handle_, UvTimer::TimerCb, timeout, 0), 0);
  return timer;
}

void UvLoop::ClearTimeout(Timer* timer) {
  UvTimer* timer_ = static_cast<UvTimer*>(timer);
  uv_close(reinterpret_cast<uv_handle_t*>(&timer_->handle_), UvTimer::CloseCb);
}

void UvIdle::IdleCb(uv_idle_t* handle) {
  UvIdle* idle = ContainerOf(&UvIdle::handle_, handle);
  CHECK_EQ(uv_idle_stop(&idle->handle_), 0);
  idle->Execute();
}

void UvIdle::CloseCb(uv_handle_t* handle) {
  UvIdle* idle =
      ContainerOf(&UvIdle::handle_, reinterpret_cast<uv_idle_t*>(handle));
  delete idle;
}

void UvTimer::TimerCb(uv_timer_t* handle) {
  UvTimer* timer = ContainerOf(&UvTimer::handle_, handle);
  timer->Execute();
}

void UvTimer::CloseCb(uv_handle_t* handle) {
  UvTimer* timer =
      ContainerOf(&UvTimer::handle_, reinterpret_cast<uv_timer_t*>(handle));
  delete timer;
}

}  // namespace ipc
}  // namespace aworker
