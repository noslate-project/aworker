#ifndef SRC_IPC_IPC_SOCKET_H_
#define SRC_IPC_IPC_SOCKET_H_
#include <vector>
#include "ipc/ipc_delegate.h"
#include "ipc/ipc_pb.h"
#include "ipc/uv_loop.h"
#include "util.h"

namespace aworker {
namespace ipc {

enum class DecodingState {
  DecodingStateError = -1,
  DecodingStateOk = 0,
  DecodingStateMore = 1,
};

class NoslatedDecoder {
 public:
  static void Init();
  NoslatedDecoder();
  void InsertBuffer(const char* base, size_t len);
  DecodingState Decode();
  inline Message* TakeContentOwnership();
  inline std::unique_ptr<MessageHeader> TakeHeaderOwnership() {
    return std::move(header_);
  }

 private:
  static const size_t MessageHeaderSize = 25;
  DecodingState DecodeRequest();
  DecodingState DecodeResponse();

  size_t read_size_ = 0;
  std::vector<uint8_t> buffer_;
  std::unique_ptr<MessageHeader> header_;
  Message* content_;
};

class SocketHolder {
 public:
  template <class T>
  static void DisconnectAndDispose(T* socket) {
    static_assert(std::is_base_of<SocketHolder, T>::value,
                  "T not derived from SocketHolder");
    static_cast<SocketHolder*>(socket)->DisconnectAndDispose();
  }
  using Pointer = DeleteFnPtr<SocketHolder, DisconnectAndDispose>;

  void OnFrame(const char* base, size_t len);
  void OnEoF();

  void Write(MessageKind mkind,
             RequestId id,
             RequestKind rkind,
             std::unique_ptr<Message> message,
             CanonicalCode code = CanonicalCode::OK);

  inline std::shared_ptr<SocketDelegate> delegate() { return delegate_; }

 protected:
  virtual bool Write(char* data, size_t len) = 0;

  template <class T>
  using PointerT = DeleteFnPtr<T, DisconnectAndDispose>;
  virtual void DisconnectAndDispose();
  SocketHolder(std::shared_ptr<EventLoop> loop,
               std::shared_ptr<SocketDelegate> delegate)
      : delegate_(delegate), loop_(loop) {}
  virtual ~SocketHolder() = default;

 private:
  void Dispatch();
  void SetReadable();
  void OnReadable();
  NoslatedDecoder decoder_;
  std::shared_ptr<SocketDelegate> delegate_;
  std::shared_ptr<EventLoop> loop_;
  Immediate* read_immediate_ = nullptr;
};

}  // namespace ipc
}  // namespace aworker

#endif  // SRC_IPC_IPC_SOCKET_H_
