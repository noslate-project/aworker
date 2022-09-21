#pragma once

#include "debug_utils.h"
#include "gtest/gtest.h"
#include "uv.h"

namespace aworker {

inline void assert_uv_loop_close(uv_loop_t* loop) {
  if (uv_loop_close(loop) == 0) return;

  PrintUvHandleInformation(loop, stderr);

  fflush(stderr);
  // Finally, abort.
  ASSERT_EQ(0 && "uv_loop_close() while having open handles", 1);
}

}  // namespace aworker
