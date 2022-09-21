#ifndef SRC_AGENT_CHANNEL_DATA_CHANNEL_H_
#define SRC_AGENT_CHANNEL_DATA_CHANNEL_H_

#include <stdio.h>
#include <string>
#include "immortal.h"
#include "utils/async_primitives.h"
#include "v8.h"

namespace aworker {
class AgentDataChannel {
 public:
  explicit AgentDataChannel(Immortal* immortal,
                            const std::string& credential,
                            bool refed)
      : immortal_(immortal), cred_(credential), refed_(refed) {}
  virtual ~AgentDataChannel() {}

  void WaitForAgent() {
    uv_loop_t* loop = immortal_->event_loop();
    do {
      uv_run(loop, UV_RUN_ONCE);
    } while (!_connected && uv_loop_alive(loop) != 0);
  }

  void set_connected() {
    // TODO(chengzhong.wcz): find a proper place to log this.
    printf("Agent Connected.\n");
    _connected = true;
    auto loop = immortal_->event_loop();
    connected_async_ = UvAsync<std::function<void()>>::Create(
        loop, std::bind(&AgentDataChannel::ConnectAsyncCb, this));
    connected_async_->Send();
    if (refed_) {
      Ref();
    }
  }
  inline bool connected() { return _connected; }
  inline Immortal* immortal() { return immortal_; }

  virtual void Ref() = 0;
  virtual void Unref() = 0;

 protected:
  void ConnectAsyncCb() { connected_async_.reset(); }

  Immortal* immortal_;
  std::string cred_;
  bool _connected = false;
  bool refed_;
  UvAsync<std::function<void()>>::Ptr connected_async_;
};

}  // namespace aworker

#endif  // SRC_AGENT_CHANNEL_DATA_CHANNEL_H_
