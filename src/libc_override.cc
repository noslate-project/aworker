#include "libc_override.h"
#include <debug_utils-inl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <csignal>
#include <cstdint>
#include <list>

static inline int aworker__getpid() {
  return syscall(__NR_getpid);
}

// disables glibc getpid caching.
pid_t getpid(void) {
  return aworker__getpid();
}

int raise(int sig) {
  return kill(getpid(), sig);
}

namespace libc_override {
using child_hook_t = void (*)(void);
std::list<child_hook_t> child_hooks_;

void RunChildHooks() {
  for (auto it : child_hooks_) {
    aworker::per_process::Debug(
        aworker::DebugCategory::OVERRIDE, "run child hook(%p)\n", it);
    it();
  }
}
}  // namespace libc_override

int pthread_atfork(void (*prepare)(void),
                   void (*parent)(void),
                   void (*child)(void)) {
  aworker::per_process::Debug(aworker::DebugCategory::OVERRIDE,
                              "pthread_atfork(%p, %p, %p)\n",
                              prepare,
                              parent,
                              child);
  libc_override::child_hooks_.push_back(child);
  return 0;
}
