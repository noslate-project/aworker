#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#include "alarm_timer.h"
#include "util.h"

namespace aworker {

using std::set;
using std::unique_ptr;

namespace {
uint64_t _next_sequence = 0;
}

static void InitSigSet(sigset_t* block, sigset_t* orig) {
  sigemptyset(block);
  sigaddset(block, SIGALRM);
  sigemptyset(orig);
}

static inline int SetNextITimer(uint64_t trigger_hrtime, uint64_t base) {
  // Raw 1e9 is double.
  const uint64_t _1e9 = 1e9;
  struct itimerval tick;
  memset(&tick, 0, sizeof(tick));

  if (trigger_hrtime >= base) {
    uint64_t delta = trigger_hrtime - base;
    tick.it_value.tv_sec = delta / _1e9;
    tick.it_value.tv_usec = (delta % _1e9) / 1e3;
  } else {
    tick.it_value.tv_usec = 1;
  }

  int res = setitimer(ITIMER_REAL, &tick, nullptr);
  if (res != 0) {
    printf("Set itimer failed: %s\n", strerror(errno));
  }

  return res;
}

static int DeleteITimer() {
  int res = setitimer(ITIMER_REAL, nullptr, nullptr);
  return res;
}

AlarmTimer::AlarmTimer(uint64_t timeout,
                       AlarmTimerHandler handler,
                       void* context)
    : _id(_next_sequence++),
      _handler(handler),
      _context(context),
      _time_when_set(uv_hrtime()),
      _time_when_trigger(_time_when_set + (timeout * 1e6)),
      _timeouted(false) {}

void AlarmTimer::Trigger() {
  _handler(this, _context);
}

inline bool AlarmTimerLathe::AlarmTimerCompare(const AlarmTimerUniquePtr& a,
                                               const AlarmTimerUniquePtr& b) {
  return (a->time_when_trigger() < b->time_when_trigger())
             ? true
             : (a->id() < b->id());
}

AlarmTimerLathe::AlarmTimerLathe() : _set(AlarmTimerCompare) {
  InitSigSet(&_block_mask, &_orig_mask);
  signal(SIGALRM, DispatchSignal);
}

AlarmTimerLathe::~AlarmTimerLathe() {
  signal(SIGALRM, SIG_DFL);
  _set.clear();
}

void AlarmTimerLathe::DispatchSignal(int sig) {
  AlarmTimerLathe& lathe = AlarmTimerLathe::Instance();
  AlarmTimerSet& timer_set = lathe._set;
  uint64_t now = uv_hrtime();

  AlarmTimer* timer;
  for (AlarmTimerSet::iterator it = timer_set.begin(); it != timer_set.end();
       it++) {
    timer = it->get();
    if (timer->time_when_trigger() > now) {
      CHECK_EQ(SetNextITimer(timer->time_when_trigger(), now), 0);
      return;
    }

    if (timer->timeouted()) continue;
    timer->set_timeouted();
    timer->Trigger();
  }
}

int AlarmTimerLathe::create_timer(uint64_t timeout,
                                  AlarmTimer::AlarmTimerHandler handler,
                                  void* context,
                                  uint64_t* timer_id) {
  int signal_op_ret = block_signal();
  CHECK_EQ(signal_op_ret, 0);

  // lazy delete previous
  AlarmTimer* timer;
  AlarmTimerSet::iterator it;
  while (!_set.empty()) {
    it = _set.begin();
    timer = it->get();
    if (!timer->timeouted()) break;
    _set.erase(it);
  }

  timer = new AlarmTimer(timeout, handler, context);
  *timer_id = timer->id();
  _set.insert(unique_ptr<AlarmTimer>(timer));

  timer = _set.begin()->get();
  uint64_t now = uv_hrtime();
  CHECK_EQ(SetNextITimer(timer->time_when_trigger(), now), 0);

  signal_op_ret = resume_signal();
  CHECK_EQ(signal_op_ret, 0);

  return 0;
}

void AlarmTimerLathe::close(uint64_t timer_id) {
  int signal_op_ret = block_signal();
  CHECK_EQ(signal_op_ret, 0);

  for (AlarmTimerSet::iterator it = _set.begin(); it != _set.end(); it++) {
    if (it->get()->id() == timer_id) {
      _set.erase(it);
      break;
    }
  }

  if (!_set.empty()) {
    AlarmTimer* timer = _set.begin()->get();
    uint64_t now = uv_hrtime();
    CHECK_EQ(SetNextITimer(timer->time_when_trigger(), now), 0);
  } else {
    CHECK_EQ(DeleteITimer(), 0);
  }

  signal_op_ret = resume_signal();
  CHECK_EQ(signal_op_ret, 0);
}

}  // namespace aworker
