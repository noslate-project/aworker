#ifndef SRC_SNAPSHOT_SNAPSHOT_BUILDER_H_
#define SRC_SNAPSHOT_SNAPSHOT_BUILDER_H_

#include <string>
#include <vector>
#include "snapshot/snapshotable.h"
#include "v8.h"

namespace aworker {

using DeserializeRequestCallback =
    void (*)(v8::Local<v8::Context> context,
             v8::Local<v8::Object> holder,
             int internal_field_index,
             const EmbedderFieldStartupData* info);

struct DeserializeRequest {
  DeserializeRequestCallback cb;
  v8::Global<v8::Object> holder;
  int index;
  const EmbedderFieldStartupData* info = nullptr;
};

class DeserializeContext {
 public:
  explicit DeserializeContext(Immortal* immortal) : immortal_(immortal) {}
  AWORKER_DISALLOW_ASSIGN_COPY(DeserializeContext);

  void EnqueueDeserializeRequest(DeserializeRequestCallback cb,
                                 v8::Local<v8::Object> holder,
                                 int index,
                                 const EmbedderFieldStartupData* info);
  void RunDeserializeRequests();

  v8::Isolate* isolate() { return immortal_->isolate(); }

 private:
  Immortal* immortal_;
  std::list<DeserializeRequest> deserialize_requests_;
};

struct NativeModuleCache {
  std::string id;
  std::vector<uint8_t> data;
};

struct SnapshotData {
  constexpr static uint32_t kMagic = 0x736e6b69;

  SnapshotData() { blob.data = nullptr; }
  ~SnapshotData() { delete[] blob.data; }
  AWORKER_DISALLOW_ASSIGN_COPY(SnapshotData);

  v8::StartupData blob;
  std::vector<size_t> isolate_data_indexes;
  std::vector<size_t> immortal_data_indexes;
  std::vector<NativeModuleCache> native_module_caches;

  std::string ToSource();
  void WriteToFile(const std::string& filename);
  static std::unique_ptr<SnapshotData> ReadFromFile(
      const std::string& filename);
};

class AworkerMainInstance;
class AworkerPlatform;
class SnapshotBuilder {
 public:
  void Generate(SnapshotData* out, int argc, char** argv);
  void Generate(SnapshotData* out,
                AworkerPlatform* platform,
                AworkerMainInstance* main_instance);
};
}  // namespace aworker

#endif  // SRC_SNAPSHOT_SNAPSHOT_BUILDER_H_
