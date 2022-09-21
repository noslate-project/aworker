#ifndef SRC_BINDING_CORE_TEXT_H_
#define SRC_BINDING_CORE_TEXT_H_

#include <string>
#include "v8.h"

namespace aworker {

class Utf8Value : public v8::String::Utf8Value {
  using Parent = v8::String::Utf8Value;

 public:
  explicit Utf8Value(v8::Isolate* isolate, v8::Local<v8::Value> value)
      : Parent(isolate, value) {}

  inline std::string ToString() const { return std::string(**this, length()); }
};

// Convenience wrapper around v8::String::NewFromOneByte
inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const char* data,
                                           int length = -1);
inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const signed char* data,
                                           int length = -1);
inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const unsigned char* data,
                                           int length = -1);
inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const std::string data);

// Convenience wrapper around v8::String::NewFromUtf8
inline v8::Local<v8::String> Utf8String(v8::Isolate* isolate,
                                        const char* data,
                                        int length = -1);
inline v8::Local<v8::String> Utf8String(v8::Isolate* isolate,
                                        const signed char* data,
                                        int length = -1);
inline v8::Local<v8::String> Utf8String(v8::Isolate* isolate,
                                        const unsigned char* data,
                                        int length = -1);
inline v8::Local<v8::String> Utf8String(v8::Isolate* isolate,
                                        const std::string data);

// https://url.spec.whatwg.org/#concept-domain-to-ascii
int32_t domainToASCII(std::string* buf,
                      const char* input,
                      size_t length,
                      bool be_strict);

}  // namespace aworker

#endif  // SRC_BINDING_CORE_TEXT_H_
