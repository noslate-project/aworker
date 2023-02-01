#ifndef SRC_AWORKER_VERSION_H_
#define SRC_AWORKER_VERSION_H_

#include <functional>
#include <string>

#define AWORKER_MAJOR_VERSION 0
#define AWORKER_MINOR_VERSION 21
#define AWORKER_PATCH_VERSION 0

#ifndef AWORKER_STRINGIFY
#define AWORKER_STRINGIFY(n) AWORKER_STRINGIFY_HELPER(n)
#define AWORKER_STRINGIFY_HELPER(n) #n
#endif

// AWORKER_TAG should be in format "-pre" or "-nightly.000000"
#ifndef AWORKER_TAG
#define AWORKER_TAG ""
#endif

#define AWORKER_VERSION_STRING                                                 \
  AWORKER_STRINGIFY(AWORKER_MAJOR_VERSION)                                     \
  "." AWORKER_STRINGIFY(AWORKER_MINOR_VERSION) "." AWORKER_STRINGIFY(          \
      AWORKER_PATCH_VERSION) AWORKER_TAG

#define AWORKER_VERSION "v" AWORKER_VERSION_STRING

namespace aworker {

class Version {
 public:
  static uint64_t hash() {
    return static_cast<uint64_t>(std::hash<std::string>{}(AWORKER_VERSION));
  }
};

}  // namespace aworker

#endif  // SRC_AWORKER_VERSION_H_
