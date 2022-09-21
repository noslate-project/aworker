#ifndef SRC_SNAPSHOT_SNAPSHOTABLE_H_
#define SRC_SNAPSHOT_SNAPSHOTABLE_H_

#include <set>
#include "base_object.h"

namespace aworker {

v8::StartupData SerializeAworkerInternalFields(v8::Local<v8::Object> holder,
                                               int index,
                                               void* data);
void DeserializeAworkerInternalFields(v8::Local<v8::Object> holder,
                                      int index,
                                      v8::StartupData payload,
                                      void* data);

// V(PropertyName, NativeType)
#define SERIALIZABLE_OBJECT_TYPES(V) V(ConverterObject, i18n::ConverterObject)

enum class EmbedderFieldObjectType : uint8_t {
  kDefault = 0,
#define V(PropertyName, NativeType) k##PropertyName,
  SERIALIZABLE_OBJECT_TYPES(V)
#undef V
};

// When serializing an embedder object, we'll serialize the native states
// into a chunk that can be mapped into a subclass of EmbedderFieldStartupData,
// and pass it into the V8 callback as the payload of StartupData.
//
// [   type   ] - EmbedderFieldObjectType (a uint8_t)
// [  length  ] - a size_t
// [    ...   ] - custom bytes of size |length - header size|
struct EmbedderFieldStartupData {
  EmbedderFieldObjectType type;
  size_t length;

  EmbedderFieldStartupData() = delete;

  static EmbedderFieldStartupData* New(EmbedderFieldObjectType type) {
    return New(type, sizeof(EmbedderFieldStartupData));
  }

  static EmbedderFieldStartupData* New(EmbedderFieldObjectType type,
                                       size_t length) {
    EmbedderFieldStartupData* result =
        reinterpret_cast<EmbedderFieldStartupData*>(::operator new[](length));
    result->type = type;
    result->length = length;
    return result;
  }
};

// An interface for snapshotable native objects to inherit from.
// Use the SERIALIZABLE_OBJECT_METHODS() macro in the class to define
// the following methods to implement:
//
// - Serialize(): This would be called during context serialization,
//   once for each embedder field of the object.
//   Allocate and construct an EmbedderFieldStartupData object that contains
//   data that can be used to deserialize native states.
// - Deserialize(): This would be called after the context is
//   deserialized and the object graph is complete, once for each
//   embedder field of the object. Use this to restore native states
//   in the object.
class SnapshotableObject : public BaseObject {
 public:
  SnapshotableObject(Immortal* immortal, v8::Local<v8::Object> object)
      : BaseObject(immortal, object) {}

  bool is_snapshotable() const override { return true; }

  virtual EmbedderFieldObjectType embedder_field_object_type() = 0;
  virtual EmbedderFieldStartupData* Serialize(int index) = 0;
  // We'll make sure that the type is set in the constructor
};

#define SERIALIZABLE_OBJECT_METHODS()                                          \
  EmbedderFieldObjectType embedder_field_object_type() override;               \
  EmbedderFieldStartupData* Serialize(int internal_field_index) override;      \
  static void Deserialize(v8::Local<v8::Context> context,                      \
                          v8::Local<v8::Object> holder,                        \
                          int internal_field_index,                            \
                          const EmbedderFieldStartupData* info);

}  // namespace aworker

#endif  // SRC_SNAPSHOT_SNAPSHOTABLE_H_
