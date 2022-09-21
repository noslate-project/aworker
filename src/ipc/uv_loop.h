#ifndef SRC_IPC_UV_LOOP_H_
#define SRC_IPC_UV_LOOP_H_
#include "uv.h"
#include "uv_helper.h"

namespace aworker {
namespace ipc {

class UvLoop : public EventLoop {
 public:
  explicit UvLoop(uv_loop_t* loop) : loop_(loop) {}
  ~UvLoop() = default;

  operator uv_loop_t*() { return reinterpret_cast<uv_loop_t*>(loop_); }

  uv_loop_t* operator->() { return reinterpret_cast<uv_loop_t*>(loop_); }

  Immediate* SetImmediate(Immediate::Callback callback) override;
  void ClearImmediate(Immediate* immediate) override;
  Timer* SetTimeout(Timer::Callback callback, uint64_t timeout) override;
  void ClearTimeout(Timer* timer) override;

 private:
  uv_loop_t* loop_;
};

}  // namespace ipc
}  // namespace aworker

#endif  // SRC_IPC_UV_LOOP_H_
