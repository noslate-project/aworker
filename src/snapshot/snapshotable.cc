#include "snapshot/snapshotable.h"
#include "binding/internal/aworker_i18n.h"
#include "debug_utils.h"
#include "snapshot/snapshot_builder.h"

namespace aworker {

v8::StartupData SerializeAworkerInternalFields(v8::Local<v8::Object> holder,
                                               int index,
                                               void* data) {
  per_process::Debug(DebugCategory::MKSNAPSHOT,
                     "Serialize internal field %d of %p\n",
                     index,
                     (*holder));

  BaseObject* ptr = reinterpret_cast<BaseObject*>(
      holder->GetAlignedPointerFromInternalField(BaseObject::kSlot));
  if (ptr == nullptr) {
    return v8::StartupData{nullptr, 0};
  }
  per_process::Debug(DebugCategory::MKSNAPSHOT,
                     "Serialize obj: %s(%p)\n",
                     ptr->GetWrapperTypeInfo()->interface_name,
                     ptr);
  if (!ptr->is_snapshotable()) {
    // DCHECK
    return v8::StartupData{nullptr, 0};
  }
  SnapshotableObject* snapshotable = static_cast<SnapshotableObject*>(ptr);

  EmbedderFieldStartupData* ser_data = snapshotable->Serialize(index);
  if (ser_data == nullptr) {
    return v8::StartupData{nullptr, 0};
  }

  // Ownership transferred
  return v8::StartupData{reinterpret_cast<const char*>(ser_data),
                         static_cast<int>(ser_data->length)};
}

void DeserializeAworkerInternalFields(v8::Local<v8::Object> holder,
                                      int index,
                                      v8::StartupData payload,
                                      void* data) {
  per_process::Debug(DebugCategory::MKSNAPSHOT,
                     "Deserialize internal field %d of %p, size=%d\n",
                     index,
                     (*holder),
                     static_cast<int>(payload.raw_size));
  if (payload.raw_size == 0) {
    holder->SetAlignedPointerInInternalField(index, nullptr);
    return;
  }

  DeserializeContext* des_context = reinterpret_cast<DeserializeContext*>(data);
  const EmbedderFieldStartupData* info =
      reinterpret_cast<const EmbedderFieldStartupData*>(payload.data);

  switch (info->type) {
#define V(PropertyName, NativeTypeName)                                        \
  case EmbedderFieldObjectType::k##PropertyName: {                             \
    per_process::Debug(                                                        \
        DebugCategory::MKSNAPSHOT,                                             \
        "Object %p is %s\n",                                                   \
        (*holder),                                                             \
        NativeTypeName::GetStaticWrapperTypeInfo()->interface_name);           \
    des_context->EnqueueDeserializeRequest(                                    \
        NativeTypeName::Deserialize, holder, index, info);                     \
    break;                                                                     \
  }
    SERIALIZABLE_OBJECT_TYPES(V)
#undef V
    default: {
      UNREACHABLE();
    }
  }
}

}  // namespace aworker
