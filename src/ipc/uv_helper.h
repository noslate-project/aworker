#ifndef SRC_IPC_UV_HELPER_H_
#define SRC_IPC_UV_HELPER_H_
#include <functional>
#include <memory>

namespace aworker {
namespace ipc {

class Timer {
 public:
  using Callback = std::function<void(Timer*)>;
  explicit Timer(Callback callback) : callback_(callback) {}

  void Execute() { callback_(this); }

 protected:
  Callback callback_;
};

class Immediate {
 public:
  using Callback = std::function<void(Immediate*)>;
  explicit Immediate(Callback callback) : callback_(callback) {}

  void Execute() { callback_(this); }

 protected:
  Callback callback_;
};

class EventLoop {
 public:
  virtual ~EventLoop() = default;

  /**
   * Use ClearImmediate to release the immediate;
   */
  virtual Immediate* SetImmediate(Immediate::Callback callback) = 0;
  virtual void ClearImmediate(Immediate* immediate) = 0;

  /**
   * Use ClearTimeout to release the timer;
   */
  virtual Timer* SetTimeout(Timer::Callback callback, uint64_t timeout) = 0;
  virtual void ClearTimeout(Timer* timer) = 0;
};

}  // namespace ipc
}  // namespace aworker

#endif  // SRC_IPC_UV_HELPER_H_
