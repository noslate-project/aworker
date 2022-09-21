#ifndef SRC_SNAPSHOT_EMBEDDED_SNAPSHOT_DATA_H_
#define SRC_SNAPSHOT_EMBEDDED_SNAPSHOT_DATA_H_

#include "snapshot/snapshot_builder.h"
#include "v8.h"

namespace aworker {

class EmbeddedSnapshotData {
 public:
  static v8::StartupData* GetSnapshotBlob();
  static const std::vector<size_t>* GetIsolateDataIndexes();
  static const std::vector<size_t>* GetImmortalDataIndexes();
  static const std::vector<NativeModuleCache>* GetNativeModuleCaches();
};

}  // namespace aworker

#endif  // SRC_SNAPSHOT_EMBEDDED_SNAPSHOT_DATA_H_
