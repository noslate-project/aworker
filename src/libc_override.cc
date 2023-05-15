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

// override glibc abort, avoid get_pid cache
// @see: https://sourceware.org/git/?p=glibc.git;a=blob;f=stdlib/abort.c;h=16a453459c9ee459b690cac7d4402ba385f52fb9;hb=d6c72f976c61d3c1465699f2bcad77e62bafe61d
#if defined(__x86_64__) || defined(_M_X64)
#define ABORT_INSTRUCTION asm ("hlt")
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#define ABORT_INSTRUCTION asm ("hlt")
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ABORT_INSTRUCTION asm ("brk\t#1000")
#else
/* No such instruction is available.  */
# define ABORT_INSTRUCTION
#endif

void abort(void) {
  raise(SIGABRT);

  while (1)
    ABORT_INSTRUCTION;
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
