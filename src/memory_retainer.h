#pragma once

#include <list>
#include <string>

#include "immortal.h"

namespace aworker {

// TODO(chengzhong.wcz): Implement memory tracker
class MemoryRetainer {
 public:
  virtual ~MemoryRetainer() = default;
};

}  // namespace aworker
