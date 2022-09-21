#ifndef SRC_BINDING_INTERNAL_PROCESS_H_
#define SRC_BINDING_INTERNAL_PROCESS_H_

#include <v8.h>
#include "external_references.h"
#include "immortal.h"

namespace aworker {
class ProcessEnvStore final : public KVStore {
 public:
  v8::MaybeLocal<v8::String> Get(v8::Isolate* isolate,
                                 v8::Local<v8::String> key) const override;
  v8::Maybe<std::string> Get(const char* key) const override;
  void Set(v8::Isolate* isolate,
           v8::Local<v8::String> key,
           v8::Local<v8::String> value) override;
  int32_t Query(v8::Isolate* isolate, v8::Local<v8::String> key) const override;
  int32_t Query(const char* key) const override;
  void Delete(v8::Isolate* isolate, v8::Local<v8::String> key) override;
  v8::Local<v8::Array> Enumerate(v8::Isolate* isolate) const override;

  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
};

v8::MaybeLocal<v8::Object> CreateEnvVarProxy(v8::Local<v8::Context> context,
                                             v8::Isolate* isolate);
}  // namespace aworker

#endif  // SRC_BINDING_INTERNAL_PROCESS_H_
