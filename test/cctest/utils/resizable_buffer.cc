#include "utils/resizable_buffer.h"
#include <gtest/gtest.h>

namespace aworker {

TEST(ResizableBuffer, Realloc) {
  ResizableBuffer buf_a;
  buf_a.Realloc(5);
  EXPECT_EQ(buf_a.byte_length(), size_t(5));
  memcpy(buf_a.buffer(), "abcd\0", buf_a.byte_length());
  buf_a.Realloc(5);

  EXPECT_EQ(strcmp(static_cast<char*>(buf_a.buffer()), "abcd"), 0);
}

TEST(ResizableBuffer, Move) {
  ResizableBuffer buf_a;
  buf_a.Realloc(5);
  EXPECT_EQ(buf_a.byte_length(), size_t(5));
  memcpy(buf_a.buffer(), "abcd\0", buf_a.byte_length());
  EXPECT_EQ(strcmp(static_cast<char*>(buf_a.buffer()), "abcd"), 0);

  ResizableBuffer buf_b = std::move(buf_a);
  EXPECT_EQ(buf_b.byte_length(), size_t(5));
  EXPECT_EQ(strcmp(static_cast<char*>(buf_b.buffer()), "abcd"), 0);

  EXPECT_EQ(buf_a.byte_length(), size_t(0));
  EXPECT_EQ(buf_a.buffer(), nullptr);
}

TEST(ResizableBuffer, Concat) {
  ResizableBuffer buf_a;
  buf_a.Realloc(5);
  memcpy(buf_a.buffer(), "abcd_", buf_a.byte_length());

  ResizableBuffer buf_b;
  buf_b.Realloc(7);
  memcpy(buf_b.buffer(), "foobar\0", buf_b.byte_length());

  buf_a.Concat(buf_b);
  EXPECT_EQ(buf_a.byte_length(), size_t(12));
  EXPECT_EQ(strcmp(static_cast<char*>(buf_a.buffer()), "abcd_foobar"), 0);
}

TEST(ResizableBuffer, Prepend) {
  ResizableBuffer buf_a;
  buf_a.Realloc(5);
  memcpy(buf_a.buffer(), "abcd\0", buf_a.byte_length());

  ResizableBuffer buf_b;
  buf_b.Realloc(6);
  memcpy(buf_b.buffer(), "foobar", buf_b.byte_length());

  buf_a.Prepend(buf_b);
  EXPECT_EQ(buf_a.byte_length(), size_t(11));
  EXPECT_EQ(strcmp(static_cast<char*>(buf_a.buffer()), "foobarabcd"), 0);
}

TEST(ResizableBuffer, PrependOverlapping) {
  ResizableBuffer buf_a;
  buf_a.Realloc(5);
  memcpy(buf_a.buffer(), "abcd\0", buf_a.byte_length());

  ResizableBuffer buf_b;
  buf_b.Realloc(3);
  memcpy(buf_b.buffer(), "foo", buf_b.byte_length());

  buf_a.Prepend(buf_b);
  EXPECT_EQ(buf_a.byte_length(), size_t(8));
  EXPECT_EQ(strcmp(static_cast<char*>(buf_a.buffer()), "fooabcd"), 0);
}
}  // namespace aworker
