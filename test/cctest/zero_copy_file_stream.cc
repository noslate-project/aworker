#include "zero_copy_file_stream.h"
#include <gtest/gtest.h>
#include <libgen.h>
#include <unistd.h>
#include <iostream>
#include "common.h"
#include "proto/test.pb.h"
#include "util.h"

namespace aworker {
std::string tmpdir() {
  auto cwd = aworker::cwd();
  auto test_dir = dirname(const_cast<char*>(cwd.c_str()));
  auto path = std::string(test_dir) + "/.tmp";
  return path;
}

TEST(ZeroCopyFileStream, Read) {
  uv_loop_t loop;
  uv_loop_init(&loop);
  auto path = cwd() + "/fixtures/text";

  bool read = false;
  UvZeroCopyInputFileStream::Create(
      &loop, path, [&read](std::unique_ptr<UvZeroCopyInputFileStream> stream) {
        ASSERT_NE(stream, nullptr);
        EXPECT_EQ(stream->ByteCount(), 0);

        std::string actual = "";
        const char* buf;
        int size;
        while (stream->Next(reinterpret_cast<const void**>(&buf), &size)) {
          actual += std::string(buf, size);
        }
        EXPECT_EQ(actual, "foobar\n");
        read = true;
      });

  uv_run(&loop, UV_RUN_DEFAULT);
  assert_uv_loop_close(&loop);
  EXPECT_TRUE(read);
}

TEST(ZeroCopyFileStream, Write) {
  uv_loop_t loop;
  uv_loop_init(&loop);
  auto path = tmpdir() + "/UvZeroCopyOutputFileStreamWrite.out";
  unlink(path.c_str());

  bool ended = false;
  {
    std::string actual = "foobar\n";
    auto stream = UvZeroCopyOutputFileStream::Create(
        &loop, path, [actual, &ended](UvZeroCopyOutputFileStream* stream) {
          EXPECT_EQ(stream->ByteCount(), static_cast<int64_t>(actual.size()));
          ended = true;
        });
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(stream->ByteCount(), 0);

    char* buf;
    int size;
    EXPECT_TRUE(stream->Next(reinterpret_cast<void**>(&buf), &size));
    ASSERT_TRUE(size >= actual.size());
    memcpy(buf, actual.c_str(), actual.size());
    stream->BackUp(size - actual.size());
  }

  uv_run(&loop, UV_RUN_DEFAULT);
  assert_uv_loop_close(&loop);

  EXPECT_TRUE(ended);
}

TEST(ZeroCopyFileStream, ProtoBufferReadWrite) {
  uv_loop_t loop;
  uv_loop_init(&loop);
  auto path = tmpdir() + "/UvZeroCopyFileStreamProtobufReadWrite.out";
  unlink(path.c_str());

  bool ended = false;
  {
    test::TestFilePage test_file_page;
    test_file_page.set_foo("bar");
    test_file_page.set_quz("qux");
    auto stream = UvZeroCopyOutputFileStream::Create(
        &loop,
        path,
        [test_file_page, &ended](UvZeroCopyOutputFileStream* stream) {
          EXPECT_EQ(stream->ByteCount(),
                    static_cast<int64_t>(test_file_page.ByteSize()));
          ended = true;
        });
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(stream->ByteCount(), 0);

    test_file_page.SerializeToZeroCopyStream(stream.get());
  }

  uv_run(&loop, UV_RUN_DEFAULT);
  ASSERT_TRUE(ended);

  bool read = false;
  UvZeroCopyInputFileStream::Create(
      &loop, path, [&read](std::unique_ptr<UvZeroCopyInputFileStream> stream) {
        ASSERT_NE(stream, nullptr);
        EXPECT_EQ(stream->ByteCount(), 0);

        test::TestFilePage test_file_page;
        test_file_page.ParseFromZeroCopyStream(stream.get());
        EXPECT_EQ(test_file_page.foo(), "bar");
        EXPECT_EQ(test_file_page.quz(), "qux");
        read = true;
      });

  uv_run(&loop, UV_RUN_DEFAULT);
  assert_uv_loop_close(&loop);
  EXPECT_TRUE(read);
}

}  // namespace aworker
