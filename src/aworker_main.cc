#include <stdio.h>
#include <string.h>
#include "aworker.h"
#include "aworker_platform.h"
#include "aworker_version.h"

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  argv = aworker::InitializeOncePerProcess(&argc, argv);

  // TODO(kaidi.zkd): pass argc / argv to user script
  auto cli = std::make_unique<aworker::CommandlineParserGroup>(argc, argv);
  if (cli->v8_options()) {
    v8::V8::SetFlagsFromString("--help", static_cast<size_t>(6));
    exit(0);
  }
  if (cli->version()) {
    printf("%s\n", AWORKER_VERSION);
    exit(0);
  }

  // CommandlineParser::Evaluate() may exit(0) if --help
  CHECK(cli->Evaluate(aworker::DEFAULT_PARSER_NAME));

  int exit_code = 0;
  {
    aworker::AworkerPlatform platform;
    aworker::AworkerPlatform::Scope use_platform(&platform);
    v8::V8::Initialize();
    {
      bool build_snapshot = cli->build_snapshot();
      aworker::AworkerMainInstance instance(&platform, std::move(cli));
      if (build_snapshot) {
        exit_code = instance.BuildSnapshot();
      } else {
        instance.Initialize();
        exit_code = instance.Start();
      }
    }
    v8::V8::Dispose();
  }

  aworker::TearDownOncePerProcess();
  return exit_code;
}
