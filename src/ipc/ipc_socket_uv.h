#ifndef SRC_IPC_IPC_SOCKET_UV_H_
#define SRC_IPC_IPC_SOCKET_UV_H_
#include "ipc/ipc_socket.h"
#include "ipc/uv_loop.h"
#include "uv.h"

namespace aworker {
namespace ipc {
class UvSocketHolder : public SocketHolder {
 public:
  using Pointer = PointerT<UvSocketHolder>;
  static Pointer Accept(uv_stream_t* server,
                        std::shared_ptr<SocketDelegate> delegate);
  static void Connect(std::shared_ptr<UvLoop> loop,
                      std::string server_address,
                      std::shared_ptr<SocketDelegate> delegate,
                      std::function<void(Pointer)> onconnect);
  bool Unref();
  bool Ref();

 protected:
  bool Write(char* data, size_t len) override;

 private:
  UvSocketHolder(std::shared_ptr<UvLoop> loop,
                 std::shared_ptr<SocketDelegate> delegate)
      : SocketHolder(loop, delegate) {}
  ~UvSocketHolder() override = default;

  static inline UvSocketHolder* From(void* handle) {
    return ContainerOf(&UvSocketHolder::pipe_, static_cast<uv_pipe_t*>(handle));
  }
  static void OnConnectCb(uv_connect_t* req, int status);
  static void OnDataReceivedCb(uv_stream_t* pipe,
                               ssize_t nread,
                               const uv_buf_t* buf);
  static void WriteCb(uv_write_t* req, int status);
  static void OnClosed(uv_handle_t* handle);

  void DisconnectAndDispose() override;

  uv_pipe_t pipe_;
};

}  // namespace ipc
}  // namespace aworker

#endif  // SRC_IPC_IPC_SOCKET_UV_H_
