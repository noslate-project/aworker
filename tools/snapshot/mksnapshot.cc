#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "aworker.h"
#include "debug_utils.h"
#include "libplatform/libplatform.h"
#include "snapshot/snapshot_builder.h"
#include "v8.h"

using aworker::SnapshotBuilder;
using v8::ArrayBuffer;
using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <path/to/output.cc>\n";
    return 1;
  }

  std::ofstream out;
  out.open(argv[1], std::ios::out | std::ios::binary);
  if (!out.is_open()) {
    std::cerr << "Cannot open " << argv[1] << "\n";
    return 1;
  }

  char argv0[] = "aworker";
  std::vector<char*> mksnapshot_argv = {argv0};
  for (int i = 0; i < argc - 2; i++) {
    mksnapshot_argv.push_back(argv[i + 2]);
  }
  int mksnapshot_argc = mksnapshot_argv.size();
  aworker::InitializeOncePerProcess(&mksnapshot_argc, mksnapshot_argv.data());

  {
    aworker::SnapshotBuilder builder;
    aworker::SnapshotData data;
    builder.Generate(&data, mksnapshot_argc, mksnapshot_argv.data());
    out << data.ToSource();
    out.close();
  }

  aworker::TearDownOncePerProcess();
  return 0;
}
