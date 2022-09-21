#ifndef SRC_PROTO_HELPER_H_
#define SRC_PROTO_HELPER_H_

#include "proto/aworker_cache.pb.h"
#include "util.h"
#include "v8.h"

namespace aworker_proto {

class StringHelper {
 public:
  static inline v8::Local<v8::Value> ToValue(const std::string* item,
                                             v8::Local<v8::Context> context);
  static inline void FromValue(v8::Local<v8::Context> context,
                               v8::Local<v8::Value> value,
                               std::string* target);
};

class BytesHelper {
 public:
  static inline v8::Local<v8::Value> ToValue(std::string* item,
                                             v8::Local<v8::Context> context);
  static inline void FromValue(v8::Local<v8::Context> context,
                               v8::Local<v8::Value> value,
                               std::string* target);
};

v8::Local<v8::Value> StringHelper::ToValue(const std::string* item,
                                           v8::Local<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::String> value =
      v8::String::NewFromUtf8(isolate, item->c_str()).ToLocalChecked();
  return value;
}

void StringHelper::FromValue(v8::Local<v8::Context> context,
                             v8::Local<v8::Value> value,
                             std::string* target) {
  v8::Isolate* isolate = context->GetIsolate();
  aworker::Utf8Value value_utf8(isolate, value.As<v8::String>());
  *target = *value_utf8;
}

v8::Local<v8::Value> BytesHelper::ToValue(std::string* item,
                                          v8::Local<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();
  std::unique_ptr<v8::BackingStore> backing_store =
      v8::ArrayBuffer::NewBackingStore(
          const_cast<char*>(item->c_str()),
          item->size(),
          [](void* data, size_t length, void* deleter_data) mutable {
            std::string* item = reinterpret_cast<std::string*>(deleter_data);
            delete item;
          },
          item);
  v8::Local<v8::ArrayBuffer> value =
      v8::ArrayBuffer::New(isolate, move(backing_store));
  return value;
}

void BytesHelper::FromValue(v8::Local<v8::Context> context,
                            v8::Local<v8::Value> value,
                            std::string* target) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Uint8Array> buffer = value.As<v8::Uint8Array>();
  *target = std::string(
      static_cast<char*>(buffer->Buffer()->GetBackingStore()->Data()) +
          buffer->ByteOffset(),
      buffer->ByteLength());
}

}  // namespace aworker_proto

#endif  // SRC_PROTO_HELPER_H_
