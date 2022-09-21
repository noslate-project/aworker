#ifndef SRC_METADATA_H_
#define SRC_METADATA_H_
#include <string>

namespace aworker {

#define AWORKER_VERSIONS_KEYS(V)                                               \
  V(aworker)                                                                   \
  V(v8)                                                                        \
  V(uv)                                                                        \
  V(zlib)

class Metadata {
 public:
  Metadata();
  Metadata(Metadata&) = delete;
  Metadata(Metadata&&) = delete;
  Metadata operator=(Metadata&) = delete;
  Metadata operator=(Metadata&&) = delete;

#define V(key) std::string key;
  AWORKER_VERSIONS_KEYS(V)
#undef V

  const std::string arch;
  const std::string platform;
};

// Per-process global
namespace per_process {
extern Metadata metadata;
}  // namespace per_process
}  // namespace aworker

#endif  // SRC_METADATA_H_
