#include "aworker_logger.h"
#include <cstdlib>

namespace aworker {

namespace per_process {
LogLevel log_level = LogLevel::kError;
}

LogLevel GetLogLevel() {
  return per_process::log_level;
}

void RefreshLogLevel() {
  auto value = std::getenv("AWORKER_LOGLEVEL");
  if (value == nullptr) {
    return;
  }

  switch (value[0]) {
    case 'D':
    case 'd': {
      per_process::log_level = LogLevel::kDebug;
      break;
    }
    case 'I':
    case 'i': {
      per_process::log_level = LogLevel::kInfo;
    }
    default: {
      per_process::log_level = LogLevel::kError;
    }
  }
}

}  // namespace aworker
