#ifndef SRC_IPC_INTERFACE_H_
#define SRC_IPC_INTERFACE_H_
#include <memory>

namespace aworker {
namespace ipc {

template <typename T>
std::shared_ptr<T> unowned_ptr(T* ptr) {
  return std::shared_ptr<T>(ptr, [](T*) {});
}

}  // namespace ipc
}  // namespace aworker

#endif  // SRC_IPC_INTERFACE_H_
