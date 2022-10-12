#ifndef TEST_CCTEST_TEST_ENV_H_
#define TEST_CCTEST_TEST_ENV_H_

#include "aworker.h"
#include "gtest/gtest.h"

class AworkerEnvironment : public ::testing::Environment {
 public:
  ~AworkerEnvironment();
  void SetUp() override;
  void TearDown() override;

  aworker::AworkerPlatform* platform() { return platform_.get(); }

  static AworkerEnvironment* env();

 private:
  std::unique_ptr<aworker::AworkerPlatform> platform_;
};

#endif  // TEST_CCTEST_TEST_ENV_H_
