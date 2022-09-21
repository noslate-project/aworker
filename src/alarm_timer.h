#ifndef SRC_ALARM_TIMER_H_
#define SRC_ALARM_TIMER_H_
#include <signal.h>
#include <stdint.h>
#include <memory>
#include <set>

#include "utils/singleton.h"
#include "uv.h"

namespace aworker {

class AlarmTimerLathe;

class AlarmTimer {
  friend class AlarmTimerLathe;
  typedef void (*AlarmTimerHandler)(AlarmTimer* timer, void* context);

 protected:
  AlarmTimer(uint64_t timeout, AlarmTimerHandler handler, void* context);
  void Trigger();

 public:
  inline uint64_t id() const { return _id; }
  inline uint64_t time_when_trigger() const { return _time_when_trigger; }
  inline size_t pos() const { return _pos; }
  inline void set_pos(size_t pos) { _pos = pos; }
  inline bool timeouted() const { return _timeouted; }
  inline void set_timeouted(bool timeout = true) { _timeouted = timeout; }

 private:
  uint64_t _id;
  size_t _pos;

  AlarmTimerHandler _handler;
  void* _context;
  uint64_t _time_when_set;
  uint64_t _time_when_trigger;
  bool _timeouted;
};

using AlarmTimerUniquePtr = std::unique_ptr<AlarmTimer>;

class AlarmTimerLathe : public Singleton<AlarmTimerLathe> {
 public:
  AlarmTimerLathe();
  ~AlarmTimerLathe();

  // !!!handler 和 context 需可重入!!!
  int create_timer(uint64_t timeout,
                   AlarmTimer::AlarmTimerHandler handler,
                   void* context,
                   uint64_t* timer_id);
  void close(uint64_t timer_id);

 private:
  static void DispatchSignal(int sig);
  static inline bool AlarmTimerCompare(const AlarmTimerUniquePtr& a,
                                       const AlarmTimerUniquePtr& b);

  using AlarmTimerSet =
      std::set<AlarmTimerUniquePtr, decltype(AlarmTimerCompare)*>;

  inline int block_signal() {
    return sigprocmask(SIG_SETMASK, &_block_mask, &_orig_mask);
  }

  inline int resume_signal() {
    return sigprocmask(SIG_SETMASK, &_orig_mask, nullptr);
  }

  sigset_t _block_mask;
  sigset_t _orig_mask;
  AlarmTimerSet _set;
};

}  // namespace aworker

#endif  // SRC_ALARM_TIMER_H_
