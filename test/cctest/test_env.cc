#include "test_env.h"

AworkerEnvironment::~AworkerEnvironment() {}

void AworkerEnvironment::SetUp() {
  int argc = 1;
  char* argv[] = {"aworker"};
  aworker::InitializeOncePerProcess(&argc, argv);
  platform_ = std::make_unique<aworker::AworkerPlatform>();
  v8::V8::InitializePlatform(platform_.get());
  v8::V8::Initialize();
}

void AworkerEnvironment::TearDown() {
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  platform_.reset();
  aworker::TearDownOncePerProcess();
}

AworkerEnvironment* AworkerEnvironment::env() {
  static AworkerEnvironment* self = new AworkerEnvironment();
  return self;
}

testing::Environment* const testing_env =
    testing::AddGlobalTestEnvironment(AworkerEnvironment::env());
