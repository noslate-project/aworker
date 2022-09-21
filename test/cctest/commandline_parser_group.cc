#include <cwalk.h>
#include <gtest/gtest.h>
#include <string>

#include "command_parser.h"

namespace aworker {

using std::string;

TEST(CommandlineParserGroup, Single) {
  const char* argv[] = {"aworker",
                        "--max-macro-task-count-per-tick=3",
                        "--report",
                        "--location=foo",
                        "--mode=seed"};
  CommandlineParserGroup group(5, const_cast<char**>(argv));
  group.Evaluate(0);

  EXPECT_EQ(group.max_macro_task_count_per_tick(), 3);
  EXPECT_EQ(group.report(), true);
  EXPECT_STREQ(group.location().c_str(), "foo");
  EXPECT_STREQ(group.mode().c_str(), "seed");
  EXPECT_EQ(group.inspect(), false);
  EXPECT_STREQ(group.agent_cred().c_str(), "");
  EXPECT_STREQ(group.preload_script().c_str(), "");
  EXPECT_EQ(group.expose_internals(), false);
}

TEST(CommandlineParserGroup, Multiple) {
  const char* argv1[] = {
      "aworker", "--report", "--location=foo", "--mode=seed"};
  const char* argv2[] = {
      "aworker", "--mode=normal", "--preload-script=./foo.js", "--inspect"};
  CommandlineParserGroup group(4, const_cast<char**>(argv1));
  group.PushCommandlineParser("second", 4, const_cast<char**>(argv2));
  group.Evaluate(0);
  group.Evaluate(1);

  string cwd = aworker::cwd();
  char buffer[PATH_MAX];

  cwk_path_get_absolute(cwd.c_str(), "./foo.js", buffer, sizeof(buffer));

  EXPECT_EQ(group.max_macro_task_count_per_tick(), 2);
  EXPECT_EQ(group.report(), true);
  EXPECT_STREQ(group.location().c_str(), "foo");
  EXPECT_STREQ(group.mode().c_str(), "normal");
  EXPECT_EQ(group.inspect(), true);
  EXPECT_STREQ(group.agent_cred().c_str(), "");
  EXPECT_STREQ(group.preload_script().c_str(), buffer);
  EXPECT_EQ(group.expose_internals(), false);
}

}  // namespace aworker
