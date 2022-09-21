#include "alarm_timer.h"
#include <gtest/gtest.h>
#include <vector>

#include "uv.h"

namespace aworker {
TEST(AlarmTimer, Trigger) {
  AlarmTimerLathe& lathe = AlarmTimerLathe::Instance();

  uint64_t timer;
  struct Context {
    uint64_t time;
    std::vector<int> nums;
  } context;
  context.time = uv_hrtime();

  lathe.create_timer(
      1000,
      [](AlarmTimer* timer, void* _context) {
        Context* context = static_cast<Context*>(_context);
        uint64_t now = uv_hrtime();
        uint64_t previous = context->time;
        EXPECT_GE(now - previous, 1000 * 1e6);
        EXPECT_LE(now - previous, 1100 * 1e6);
        context->nums.push_back(-1);
      },
      &context,
      &timer);

  for (int i = 0; i < 20; i++) {
    usleep(100 * 1000);
    context.nums.push_back(i);
  }

  int found = 0;
  for (int i = 0; i < 20; i++) {
    if (context.nums[i] == -1) {
      found = i;
      break;
    }
  }

  EXPECT_EQ(context.nums.size(), 21);
  EXPECT_TRUE(found >= 9 && found <= 10);
}

TEST(AlarmTimer, TriggerTwiceSame) {
  AlarmTimerLathe& lathe = AlarmTimerLathe::Instance();

  uint64_t timer;
  struct Context {
    uint64_t time;
    std::vector<int> nums;
  } context;
  context.time = uv_hrtime();

  lathe.create_timer(
      1000,
      [](AlarmTimer* timer, void* _context) {
        Context* context = static_cast<Context*>(_context);
        uint64_t now = uv_hrtime();
        uint64_t previous = context->time;
        EXPECT_GE(now - previous, 1000 * 1e6);
        EXPECT_LE(now - previous, 1100 * 1e6);
        context->nums.push_back(-1);
      },
      &context,
      &timer);
  lathe.create_timer(
      1000,
      [](AlarmTimer* timer, void* _context) {
        Context* context = static_cast<Context*>(_context);
        uint64_t now = uv_hrtime();
        uint64_t previous = context->time;
        EXPECT_GE(now - previous, 1000 * 1e6);
        EXPECT_LE(now - previous, 1100 * 1e6);
        context->nums.push_back(-2);
      },
      &context,
      &timer);

  for (int i = 0; i < 20; i++) {
    usleep(100 * 1000);
    context.nums.push_back(i);
  }

  int found1 = 0;
  int found2 = 2;
  for (int i = 0; i < 20; i++) {
    if (context.nums[i] == -1) {
      found1 = i;
    } else if (context.nums[i] == -2) {
      found2 = i;
      break;
    }
  }

  EXPECT_EQ(context.nums.size(), 22);
  EXPECT_TRUE(found1 >= 9 && found1 <= 10);
  EXPECT_EQ(found1 + 1, found2);
}

TEST(AlarmTimer, TriggerTwiceDiff) {
  AlarmTimerLathe& lathe = AlarmTimerLathe::Instance();

  uint64_t timer;
  struct Context {
    uint64_t time;
    std::vector<int> nums;
  } context;
  context.time = uv_hrtime();

  lathe.create_timer(
      1000,
      [](AlarmTimer* timer, void* _context) {
        Context* context = static_cast<Context*>(_context);
        uint64_t now = uv_hrtime();
        uint64_t previous = context->time;
        EXPECT_GE(now - previous, 1000 * 1e6);
        EXPECT_LE(now - previous, 1100 * 1e6);
        context->nums.push_back(-1);
      },
      &context,
      &timer);
  lathe.create_timer(
      1200,
      [](AlarmTimer* timer, void* _context) {
        Context* context = static_cast<Context*>(_context);
        uint64_t now = uv_hrtime();
        uint64_t previous = context->time;
        EXPECT_GE(now - previous, 1200 * 1e6);
        EXPECT_LE(now - previous, 1300 * 1e6);
        context->nums.push_back(-2);
      },
      &context,
      &timer);

  for (int i = 0; i < 20; i++) {
    usleep(100 * 1000);
    context.nums.push_back(i);
  }

  int found1 = 0;
  int found2 = 2;
  for (int i = 0; i < 20; i++) {
    if (context.nums[i] == -1) {
      found1 = i;
    } else if (context.nums[i] == -2) {
      found2 = i;
      break;
    }
  }

  EXPECT_EQ(context.nums.size(), 22);
  EXPECT_TRUE(found1 >= 9 && found1 <= 10);
  EXPECT_TRUE(found2 >= 11 && found2 <= 12);
}

TEST(AlarmTimer, Close) {
  AlarmTimerLathe& lathe = AlarmTimerLathe::Instance();

  uint64_t timer;
  struct Context {
    uint64_t time;
    std::vector<int> nums;
  } context;
  context.time = uv_hrtime();

  lathe.create_timer(
      1000,
      [](AlarmTimer* timer, void* _context) { EXPECT_TRUE(false); },
      &context,
      &timer);

  for (int i = 0; i < 20; i++) {
    if (i == 3) {
      lathe.close(timer);
    }

    usleep(100 * 1000);
    context.nums.push_back(i);
  }

  for (int i = 0; i < 20; i++) {
    EXPECT_EQ(context.nums[i], i);
  }

  EXPECT_EQ(context.nums.size(), 20);
}

TEST(AlarmTimer, CloseUnexists) {
  AlarmTimerLathe& lathe = AlarmTimerLathe::Instance();

  uint64_t timer;
  struct Context {
    uint64_t time;
    std::vector<int> nums;
  } context;
  context.time = uv_hrtime();

  lathe.create_timer(
      1000,
      [](AlarmTimer* timer, void* _context) {
        Context* context = static_cast<Context*>(_context);
        uint64_t now = uv_hrtime();
        uint64_t previous = context->time;
        EXPECT_GE(now - previous, 1000 * 1e6);
        EXPECT_LE(now - previous, 1100 * 1e6);
        context->nums.push_back(-1);
      },
      &context,
      &timer);

  for (int i = 0; i < 20; i++) {
    if (i == 3) {
      lathe.close(timer - 1);
    }

    usleep(100 * 1000);
    context.nums.push_back(i);
  }

  int found = 0;
  for (int i = 0; i < 20; i++) {
    if (context.nums[i] == -1) {
      found = i;
      break;
    }
  }

  EXPECT_EQ(context.nums.size(), 21);
  EXPECT_TRUE(found >= 9 && found <= 10);
}
}  // namespace aworker
