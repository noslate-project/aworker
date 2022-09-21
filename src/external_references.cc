#include "external_references.h"
#include <cinttypes>
#include <vector>
#include "aworker_binding.h"
#include "binding/internal/process.h"
#include "native_module_manager.h"
#include "util.h"

namespace aworker {

namespace per_process {
ExternalReferenceRegistry external_reference_registry;
}

const std::vector<intptr_t>& ExternalReferenceRegistry::external_references() {
  return external_references_;
}

void ExternalReferenceRegistry::Collect() {
  this->Register(binding::GetBinding);
  this->Register(NativeModuleManager::GetNativeModuleFunction);

  for (auto it : per_process::bindings) {
    it->external_reference_func(this);
  }

  ProcessEnvStore::RegisterExternalReferences(this);
  external_references_.push_back(reinterpret_cast<intptr_t>(nullptr));
}

}  // namespace aworker
