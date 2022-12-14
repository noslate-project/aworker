#include <sys/syscall.h>
#include <unistd.h>
#include <csignal>
#include <cstdint>

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
