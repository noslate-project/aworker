#include "utils/result.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <csignal>
#include <iostream>
#include <string>

namespace aworker {

namespace {
class Foo {
 public:
  explicit Foo(int bar) : bar() {}
  // Disallow copy
  Foo(const Foo& foo) = delete;
  void operator=(const Foo& foo) = delete;
  // Allow move
  Foo(const Foo&& foo) { bar = foo.bar; }
  void operator=(const Foo&& foo) { bar = foo.bar; }

  bool operator==(const Foo& other) const { return bar == other.bar; }

  int bar;
};
}  // namespace

TEST(Result, Ok) {
  using result_type = Result<bool, std::string>;
  result_type result = result_type::Ok(true);
  EXPECT_EQ(result.IsOk(), true);
  EXPECT_EQ(result.IsError(), false);
  EXPECT_EQ(result.ToValueChecked(), true);

  bool value;
  EXPECT_EQ(result.ToValue(&value), true);
  std::string error;
  EXPECT_EQ(result.ToError(&error), false);
  EXPECT_EQ(value, true);

  EXPECT_EXIT(result.ToErrorChecked(),
              ::testing::KilledBySignal(SIGABRT),
              "Assertion `IsError\\(\\)` failed");
}

TEST(Result, Error) {
  using result_type = Result<bool, std::string>;
  std::string init = "foobar";
  result_type result = result_type::Error(init);
  EXPECT_EQ(result.IsOk(), false);
  EXPECT_EQ(result.IsError(), true);
  EXPECT_EQ(result.ToErrorChecked(), "foobar");

  result = result_type::Error("foobar");
  bool value;
  EXPECT_EQ(result.ToValue(&value), false);
  std::string error;
  EXPECT_EQ(result.ToError(&error), true);
  EXPECT_EQ(error, "foobar");

  EXPECT_EXIT(result.ToValueChecked(),
              ::testing::KilledBySignal(SIGABRT),
              "Assertion `IsOk\\(\\)` failed");
}

TEST(ResultNonCopyableConstruct, Ok) {
  using result_type = Result<Foo, std::string>;
  Foo init{1};
  result_type result = result_type::Ok(std::move(init));
  EXPECT_EQ(result.IsOk(), true);
  EXPECT_EQ(result.ToValueChecked(), Foo(1));

  Foo value{-1};
  EXPECT_EQ(result.ToValue(&value), true);
}
}  // namespace aworker
