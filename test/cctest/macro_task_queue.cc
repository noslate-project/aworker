#include "macro_task_queue.h"
#include <gtest/gtest.h>
#include <unistd.h>
#include <iostream>
#include "common.h"
#include "util.h"

namespace aworker {
namespace {
class TestTask : public MacroTask {
 public:
  TestTask(int* count, int times) : _count(count), _times(times) {}
  void OnWorkTick() {
    *_count += 1;
    if (*_count == _times) {
      Done();
    }
  }

 private:
  int* _count;
  int _times;
};

}  // namespace

TEST(MacroTaskQueue, EachTickShouldScheduleNextTickImmediately) {
  uv_loop_t loop;
  uv_loop_init(&loop);
  uv_timer_t timer;

  {
    std::shared_ptr<MacroTaskQueue> macro_task_queue =
        MacroTaskQueue::Create(&loop, 1);
    int count = 0;
    // Tick twice.
    macro_task_queue->Enqueue(std::make_unique<TestTask>(&count, 2));
    // Schedule a timer to check if the task_queue is drained before the
    // timeout.
    uv_timer_init(&loop, &timer);
    timer.data = &count;
    uv_timer_start(
        &timer,
        [](uv_timer_t* timer) {
          int* count = reinterpret_cast<int*>(timer->data);
          EXPECT_EQ(*count, 2);
          uv_close(reinterpret_cast<uv_handle_t*>(timer), nullptr);
        },
        100,
        0);

    uv_run(&loop, UV_RUN_DEFAULT);
    EXPECT_EQ(count, 2);
  }

  assert_uv_loop_close(&loop);
}
}  // namespace aworker
