#include "aworker_platform.h"
#include <gtest/gtest.h>
#include "common.h"

namespace aworker {

#define _100ms 100 * 1000

class TestTask : public v8::Task {
 public:
  TestTask(int* count, bool* done, v8::TaskRunner* runner)
      : _count(count), _done(done), _runner(runner) {}
  void Run() override {
    (*_count)++;
    if (*_done) {
      return;
    }
    _runner->PostTask(std::make_unique<TestTask>(_count, _done, _runner));
  }

 private:
  int* _count;
  bool* _done;
  v8::TaskRunner* _runner;
};

class TestDelayedTask : public v8::Task {
 public:
  TestDelayedTask(int* count, bool* done, v8::TaskRunner* runner)
      : _count(count), _done(done), _runner(runner) {}
  void Run() override {
    (*_count)++;
    if (*_done) {
      return;
    }
    _runner->PostDelayedTask(std::make_unique<TestTask>(_count, _done, _runner),
                             0.01 /* 10ms */);
  }

 private:
  int* _count;
  bool* _done;
  v8::TaskRunner* _runner;
};

TEST(AworkerPlatformForegroundTaskRunner, PostTask) {
  AworkerPlatform platform(AworkerPlatform::kSingleThread);
  uv_loop_t* loop = platform.loop();

  // uv_run will not spin the loop if there is no ref-ed handles/reqs.
  uv_idle_t idle;
  ASSERT_EQ(uv_idle_init(loop, &idle), 0);
  ASSERT_EQ(uv_idle_start(&idle, [](uv_idle_t* handle) {}), 0);

  int count = 0;
  bool done = false;
  auto runner = platform.GetForegroundTaskRunner(nullptr);
  runner->PostTask(std::make_unique<TestTask>(&count, &done, runner.get()));

  usleep(_100ms);
  uv_run(loop, UV_RUN_NOWAIT);
  EXPECT_EQ(count, 1);

  // disable re-post task.
  done = true;
  // task runner re-scheduled timer has to be at least 1ms later.
  usleep(_100ms);
  uv_run(loop, UV_RUN_NOWAIT);
  EXPECT_EQ(count, 2);

  // no tasks should be posted for now.
  usleep(_100ms);
  uv_run(loop, UV_RUN_NOWAIT);
  EXPECT_EQ(count, 2);

  // post a new task after the runner has been drained.
  runner->PostTask(std::make_unique<TestTask>(&count, &done, runner.get()));

  usleep(_100ms);
  uv_run(loop, UV_RUN_NOWAIT);
  EXPECT_EQ(count, 3);

  // close the ref-ed handle so that the disposure of aworker platform will
  // not loop forever
  uv_close(reinterpret_cast<uv_handle_t*>(&idle), nullptr);
}

TEST(AworkerPlatformForegroundTaskRunner, PostDelayedTask) {
  AworkerPlatform platform(AworkerPlatform::kSingleThread);
  uv_loop_t* loop = platform.loop();

  // uv_run will not spin the loop if there is no ref-ed handles/reqs.
  uv_idle_t idle;
  ASSERT_EQ(uv_idle_init(loop, &idle), 0);
  ASSERT_EQ(uv_idle_start(&idle, [](uv_idle_t* handle) {}), 0);

  int count = 0;
  bool done = false;
  auto runner = platform.GetForegroundTaskRunner(nullptr);
  runner->PostDelayedTask(
      std::make_unique<TestDelayedTask>(&count, &done, runner.get()),
      0.01 /* 10ms */);

  usleep(_100ms);
  uv_run(loop, UV_RUN_NOWAIT);
  EXPECT_EQ(count, 1);

  // disable re-post task.
  done = true;
  // task runner re-scheduled timer has to be at least 1ms later.
  usleep(_100ms);
  uv_run(loop, UV_RUN_NOWAIT);
  EXPECT_EQ(count, 2);

  // no tasks should be posted for now.
  usleep(_100ms);
  uv_run(loop, UV_RUN_NOWAIT);
  EXPECT_EQ(count, 2);

  // post a new task after the runner has been drained.
  runner->PostDelayedTask(
      std::make_unique<TestDelayedTask>(&count, &done, runner.get()),
      0.01 /* 10ms */);
  usleep(_100ms);
  uv_run(loop, UV_RUN_NOWAIT);
  EXPECT_EQ(count, 3);

  // close the ref-ed handle so that the disposure of aworker platform will
  // not loop forever
  uv_close(reinterpret_cast<uv_handle_t*>(&idle), nullptr);
}

TEST(AworkerPlatformForegroundTaskRunner, Interwined) {
  AworkerPlatform platform(AworkerPlatform::kSingleThread);
  uv_loop_t* loop = platform.loop();

  // uv_run will not spin the loop if there is no ref-ed handles/reqs.
  uv_idle_t idle;
  ASSERT_EQ(uv_idle_init(loop, &idle), 0);
  ASSERT_EQ(uv_idle_start(&idle, [](uv_idle_t* handle) {}), 0);

  int count = 0;
  bool done = false;
  auto runner = platform.GetForegroundTaskRunner(nullptr);
  runner->PostDelayedTask(
      std::make_unique<TestDelayedTask>(&count, &done, runner.get()),
      1 /* 1000ms */);
  runner->PostTask(std::make_unique<TestTask>(&count, &done, runner.get()));
  // Timeline:
  // |- 100ms
  // |- immediate task
  // |- 1s
  // |- immediate task
  // |- delayed task
  // |- 100ms
  // |- immediate task
  // |- delayed task
  // |
  // |- immediate task

  usleep(_100ms);
  uv_run(loop, UV_RUN_NOWAIT);
  EXPECT_EQ(count, 1);

  // task runner re-scheduled timer has to be at least 1ms later.
  sleep(1);
  uv_run(loop, UV_RUN_NOWAIT);
  // both delayed task and immediate task were executed.
  EXPECT_EQ(count, 3);

  done = true;
  // task runner re-scheduled timer has to be at least 1ms later.
  usleep(_100ms);
  uv_run(loop, UV_RUN_NOWAIT);
  // both delayed task and immediate task were executed.
  EXPECT_EQ(count, 5);

  // no tasks should be posted for now.
  usleep(_100ms);
  uv_run(loop, UV_RUN_NOWAIT);
  EXPECT_EQ(count, 5);

  // post a new task after the runner has been drained.
  runner->PostDelayedTask(
      std::make_unique<TestDelayedTask>(&count, &done, runner.get()),
      0.01 /* 10ms */);
  usleep(_100ms);
  uv_run(loop, UV_RUN_NOWAIT);
  EXPECT_EQ(count, 6);

  // close the ref-ed handle so that the disposure of aworker platform will
  // not loop forever
  uv_close(reinterpret_cast<uv_handle_t*>(&idle), nullptr);
}

TEST(AworkerPlatformForegroundTaskRunner, InterwinedDuringReschedule) {
  AworkerPlatform platform(AworkerPlatform::kSingleThread);
  uv_loop_t* loop = platform.loop();

  // uv_run will not spin the loop if there is no ref-ed handles/reqs.
  uv_idle_t idle;
  ASSERT_EQ(uv_idle_init(loop, &idle), 0);
  ASSERT_EQ(uv_idle_start(&idle, [](uv_idle_t* handle) {}), 0);

  int count = 0;
  bool done = false;
  auto runner = platform.GetForegroundTaskRunner(nullptr);
  runner->PostDelayedTask(
      std::make_unique<TestDelayedTask>(&count, &done, runner.get()),
      0.1 /* 100ms */);
  // Timeline:
  // |- 100ms
  // |- delayed task
  // |- 100ms
  // |- immediate task
  // |- delayed task
  // |
  // |- immediate task

  sleep(1);
  uv_run(loop, UV_RUN_NOWAIT);
  // both delayed task and immediate task were executed.
  EXPECT_EQ(count, 1);

  runner->PostTask(std::make_unique<TestTask>(&count, &done, runner.get()));

  done = true;
  // task runner re-scheduled timer has to be at least 1ms later.
  usleep(_100ms);
  uv_run(loop, UV_RUN_NOWAIT);
  // both delayed task and immediate task were executed.
  EXPECT_EQ(count, 3);

  // no tasks should be posted for now.
  usleep(_100ms);
  uv_run(loop, UV_RUN_NOWAIT);
  EXPECT_EQ(count, 3);

  // close the ref-ed handle so that the disposure of aworker platform will
  // not loop forever
  uv_close(reinterpret_cast<uv_handle_t*>(&idle), nullptr);
}

TEST(AworkerPlatformSingleThread, NumberOfWorkerThreads) {
  AworkerPlatform platform(AworkerPlatform::kSingleThread);
  EXPECT_EQ(platform.NumberOfWorkerThreads(), 0);
}

TEST(AworkerPlatformMultiThread, NumberOfWorkerThreads) {
  AworkerPlatform platform(AworkerPlatform::kMultiThread);
  EXPECT_GT(platform.NumberOfWorkerThreads(), 0);
}

}  // namespace aworker
