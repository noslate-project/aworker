#ifndef SRC_BINDING_CORE_TEXT_INL_H_
#define SRC_BINDING_CORE_TEXT_INL_H_

#include "binding/core/text.h"

namespace aworker {

// Convenience wrapper around v8::String::NewFromOneByte
inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const char* data,
                                           int length) {
  return v8::String::NewFromOneByte(isolate,
                                    reinterpret_cast<const uint8_t*>(data),
                                    v8::NewStringType::kNormal,
                                    length)
      .ToLocalChecked();
}

inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const signed char* data,
                                           int length) {
  return v8::String::NewFromOneByte(isolate,
                                    reinterpret_cast<const uint8_t*>(data),
                                    v8::NewStringType::kNormal,
                                    length)
      .ToLocalChecked();
}

inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const unsigned char* data,
                                           int length) {
  return v8::String::NewFromOneByte(
             isolate, data, v8::NewStringType::kNormal, length)
      .ToLocalChecked();
}

inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const std::string data) {
  return v8::String::NewFromOneByte(
             isolate,
             reinterpret_cast<const uint8_t*>(data.c_str()),
             v8::NewStringType::kNormal,
             data.length())
      .ToLocalChecked();
}

inline v8::Local<v8::String> Utf8String(v8::Isolate* isolate,
                                        const char* data,
                                        int length) {
  return v8::String::NewFromUtf8(
             isolate, data, v8::NewStringType::kNormal, length)
      .ToLocalChecked();
}

inline v8::Local<v8::String> Utf8String(v8::Isolate* isolate,
                                        const signed char* data,
                                        int length) {
  return v8::String::NewFromUtf8(isolate,
                                 reinterpret_cast<const char*>(data),
                                 v8::NewStringType::kNormal,
                                 length)
      .ToLocalChecked();
}

inline v8::Local<v8::String> Utf8String(v8::Isolate* isolate,
                                        const unsigned char* data,
                                        int length) {
  return v8::String::NewFromUtf8(isolate,
                                 reinterpret_cast<const char*>(data),
                                 v8::NewStringType::kNormal,
                                 length)
      .ToLocalChecked();
}

inline v8::Local<v8::String> Utf8String(v8::Isolate* isolate,
                                        const std::string data) {
  return v8::String::NewFromUtf8(
             isolate, data.c_str(), v8::NewStringType::kNormal, data.length())
      .ToLocalChecked();
}

}  // namespace aworker

#endif  // SRC_BINDING_CORE_TEXT_INL_H_
