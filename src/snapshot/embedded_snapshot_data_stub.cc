#include "snapshot/embedded_snapshot_data.h"
#include "v8.h"

namespace aworker {
v8::StartupData* EmbeddedSnapshotData::GetSnapshotBlob() {
  return nullptr;
}

const std::vector<size_t>* EmbeddedSnapshotData::GetIsolateDataIndexes() {
  return nullptr;
}

const std::vector<size_t>* EmbeddedSnapshotData::GetImmortalDataIndexes() {
  return nullptr;
}

const std::vector<NativeModuleCache>*
EmbeddedSnapshotData::GetNativeModuleCaches() {
  return nullptr;
}

}  // namespace aworker
