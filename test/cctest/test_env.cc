#include "aworker.h"
#include "gtest/gtest.h"

class AworkerEnvironment : public ::testing::Environment {
 public:
  ~AworkerEnvironment() override {}

  void SetUp() override {
    int argc = 1;
    char* argv[] = {"aworker"};
    aworker::InitializeOncePerProcess(&argc, argv);
  }

  void TearDown() override { aworker::TearDownOncePerProcess(); }
};

testing::Environment* const foo_env =
    testing::AddGlobalTestEnvironment(new AworkerEnvironment);
