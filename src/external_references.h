#pragma once

#include <cinttypes>
#include <vector>
#include "v8.h"

namespace aworker {

// This class manages the external references from the V8 heap
// to the C++ addresses in aworker.
class ExternalReferenceRegistry {
 public:
  void Collect();

#define ALLOWED_EXTERNAL_REFERENCE_TYPES(V)                                    \
  V(v8::FunctionCallback)                                                      \
  V(v8::AccessorGetterCallback)                                                \
  V(v8::AccessorSetterCallback)                                                \
  V(v8::AccessorNameGetterCallback)                                            \
  V(v8::AccessorNameSetterCallback)                                            \
  V(v8::GenericNamedPropertyDefinerCallback)                                   \
  V(v8::GenericNamedPropertyDeleterCallback)                                   \
  V(v8::GenericNamedPropertyEnumeratorCallback)                                \
  V(v8::GenericNamedPropertyQueryCallback)                                     \
  V(v8::GenericNamedPropertySetterCallback)

#define V(ExternalReferenceType)                                               \
  void Register(ExternalReferenceType addr) { RegisterT(addr); }
  ALLOWED_EXTERNAL_REFERENCE_TYPES(V)
#undef V

  // This can be called only once.
  const std::vector<intptr_t>& external_references();

  bool is_empty() {
    return external_references_.empty();
  }

 private:
  template <typename T>
  void RegisterT(T* address) {
    external_references_.push_back(reinterpret_cast<intptr_t>(address));
  }
  bool is_finalized_ = false;
  std::vector<intptr_t> external_references_;
};

namespace per_process {
extern ExternalReferenceRegistry external_reference_registry;
}

}  // namespace aworker
