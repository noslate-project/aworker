#include <ctype.h>
#include <modp_b64.h>
#include <string.h>
#include <unicode/utypes.h>

#include <cmath>
#include <codecvt>
#include <locale>
#include <string>

#include "aworker_binding.h"
#include "binding/core/text.h"
#include "error_handling.h"
#include "immortal.h"
#include "util.h"
#include "utils/resizable_buffer.h"

namespace aworker {
using v8::ArrayBuffer;
using v8::BackingStore;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Value;

size_t hex_encode(const char* src, size_t slen, char* dst, size_t dlen) {
  // We know how much we'll write, just make sure that there's space.
  CHECK(dlen >= slen * 2 && "not enough space provided for hex encode");

  dlen = slen * 2;
  for (uint32_t i = 0, k = 0; k < dlen; i += 1, k += 2) {
    static const char hex[] = "0123456789abcdef";
    uint8_t val = static_cast<uint8_t>(src[i]);
    dst[k + 0] = hex[val >> 4];
    dst[k + 1] = hex[val & 15];
  }

  return dlen;
}

std::string hex_encode(const char* src, size_t slen) {
  size_t dlen = slen * 2;
  std::string dst(dlen, '\0');
  hex_encode(src, slen, &dst[0], dlen);
  return dst;
}

constexpr char16_t kUnicodeReplacementCharacter = 0xFFFD;

#define CHAR_TEST(bits, name, expr)                                            \
  template <typename T>                                                        \
  bool name(const T ch) {                                                      \
    static_assert(sizeof(ch) >= (bits) / 8,                                    \
                  "Character must be wider than " #bits " bits");              \
    return (expr);                                                             \
  }

// If a UTF-16 character is a low/trailing surrogate.
CHAR_TEST(16, IsUnicodeTrail, (ch & 0xFC00) == 0xDC00)

// If a UTF-16 character is a surrogate.
CHAR_TEST(16, IsUnicodeSurrogate, (ch & 0xF800) == 0xD800)

// If a UTF-16 surrogate is a low/trailing one.
CHAR_TEST(16, IsUnicodeSurrogateTrail, (ch & 0x400) != 0)

// https://url.spec.whatwg.org/#concept-domain-to-ascii
AWORKER_METHOD(DomainToASCII) {
  Immortal* immortal = Immortal::GetCurrent(info);
  auto isolate = immortal->isolate();
  HandleScope scope(isolate);

  String::Utf8Value val(isolate, info[0]);

  std::string buf = "";
  int32_t len = domainToASCII(&buf, *val, val.length(), false);

  if (len < 0) {
    return ThrowException(isolate, "Cannot convert to IDNA ASCII");
  }

  info.GetReturnValue().Set(
      String::NewFromUtf8(isolate, buf.c_str(), v8::NewStringType::kNormal, len)
          .ToLocalChecked());
}

// Refs:
// https://github.com/nodejs/node/blob/8e84d566eababe9585864678e729c735a19e0085/src/node_url.cc#L1739
AWORKER_METHOD(ToUSVString) {
  CHECK_GE(info.Length(), 2);
  CHECK(info[0]->IsString());
  CHECK(info[1]->IsNumber());

  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  Local<String> v8_string = info[0].As<String>();

  ResizableBuffer buff((v8_string->Length() + 1) * sizeof(uint16_t));
  uint16_t* value = static_cast<uint16_t*>(buff.buffer());
  size_t length = v8_string->Write(
      isolate, value, 0, v8_string->Length() + 1, String::NO_NULL_TERMINATION);

  int64_t start = info[1]->IntegerValue(immortal->context()).FromJust();
  CHECK_GE(start, 0);

  for (size_t i = start; i < length; i++) {
    char16_t c = value[i];
    if (!IsUnicodeSurrogate(c)) {
      continue;
    } else if (IsUnicodeSurrogateTrail(c) || i == length - 1) {
      value[i] = kUnicodeReplacementCharacter;
    } else {
      char16_t d = value[i + 1];
      if (IsUnicodeTrail(d)) {
        i++;
      } else {
        value[i] = kUnicodeReplacementCharacter;
      }
    }
  }

  info.GetReturnValue().Set(
      String::NewFromTwoByte(isolate, value, v8::NewStringType::kNormal, length)
          .ToLocalChecked());
}

AWORKER_METHOD(BufferEncodeHex) {
  CHECK(info[0]->IsArrayBuffer());
  Local<ArrayBuffer> ab = info[0].As<ArrayBuffer>();
  std::shared_ptr<v8::BackingStore> bs = ab->GetBackingStore();
  CHECK(info[1]->IsUint32());
  uint32_t offset = info[1].As<Uint32>()->Value();
  CHECK(info[2]->IsUint32());
  uint32_t byte_length = info[2].As<Uint32>()->Value();
  CHECK(offset + byte_length <= bs->ByteLength());

  size_t dlen = byte_length * 2;
  Local<ArrayBuffer> dab = ArrayBuffer::New(info.GetIsolate(), dlen);
  std::shared_ptr<BackingStore> dbs = dab->GetBackingStore();

  hex_encode(static_cast<const char*>(bs->Data()) + offset,
             byte_length,
             const_cast<char*>(static_cast<const char*>(dbs->Data())),
             dlen);
  info.GetReturnValue().Set(dab);
}

AWORKER_METHOD(BufferEncodeBase64) {
  Isolate* isolate = info.GetIsolate();

  CHECK(info[0]->IsArrayBuffer());
  Local<ArrayBuffer> ab = info[0].As<ArrayBuffer>();
  std::shared_ptr<v8::BackingStore> bs = ab->GetBackingStore();
  CHECK(info[1]->IsUint32());
  uint32_t offset = info[1].As<Uint32>()->Value();
  CHECK(info[2]->IsUint32());
  uint32_t byte_length = info[2].As<Uint32>()->Value();
  CHECK(offset + byte_length <= bs->ByteLength());

  size_t dlen = modp_b64_encode_len(byte_length);
  ResizableBuffer* buf = new ResizableBuffer(dlen);

  modp_b64_encode(static_cast<char*>(**buf),
                  static_cast<const char*>(bs->Data()) + offset,
                  byte_length);

  std::shared_ptr<v8::BackingStore> backing_store =
      ArrayBuffer::NewBackingStore(
          **buf,
          dlen - 1,  //> Do not count last \0;
          [](void* data, size_t length, void* deleter_data) {
            delete static_cast<ResizableBuffer*>(deleter_data);
          },
          buf);
  Local<ArrayBuffer> array_buffer = ArrayBuffer::New(isolate, backing_store);
  info.GetReturnValue().Set(array_buffer);
}

AWORKER_METHOD(AToB) {
  Immortal* immortal = Immortal::GetCurrent(info);
  auto isolate = immortal->isolate();
  HandleScope scope(isolate);

  if (!info.Length()) {
    ThrowException(isolate,
                   "1 argument required, but only 0 present.",
                   ExceptionType::kTypeError);
    return;
  }

  Local<String> string_to_be_decoded = info[0].As<String>();
  String::Utf8Value string_to_be_decoded_utf8(isolate, string_to_be_decoded);
  size_t orig_len = string_to_be_decoded_utf8.length();
  ResizableBuffer buf(modp_b64_decode_len(orig_len) + 1);

  size_t dest_len = modp_b64_decode(
      static_cast<char*>(*buf), *string_to_be_decoded_utf8, orig_len);
  if (dest_len == MODP_B64_ERROR) {
    ThrowException(isolate,
                   "The string to be decoded is not correctly encoded.",
                   ExceptionType::kTypeError);
    return;
  }
  CHECK_LE(dest_len, buf.byte_length());

  MaybeLocal<String> maybe_dest = String::NewFromUtf8(
      isolate, static_cast<char*>(*buf), NewStringType::kNormal, dest_len);
  CHECK(!maybe_dest.IsEmpty());
  info.GetReturnValue().Set(maybe_dest.ToLocalChecked());
}

AWORKER_METHOD(BToA) {
  Immortal* immortal = Immortal::GetCurrent(info);
  auto isolate = immortal->isolate();
  HandleScope scope(isolate);

  if (!info.Length()) {
    ThrowException(isolate,
                   "1 argument required, but only 0 present.",
                   ExceptionType::kTypeError);
    return;
  }

  Local<String> string_to_be_encoded = info[0].As<String>();
  String::Utf8Value string_to_be_encoded_utf8(isolate, string_to_be_encoded);
  size_t orig_len = string_to_be_encoded_utf8.length();
  ResizableBuffer buf(modp_b64_encode_strlen(orig_len) + 1);

  size_t dest_len = modp_b64_encode(
      static_cast<char*>(*buf), *string_to_be_encoded_utf8, orig_len);
  CHECK_LE(dest_len, buf.byte_length());
  MaybeLocal<String> maybe_dest = String::NewFromUtf8(
      isolate, static_cast<char*>(*buf), NewStringType::kNormal, dest_len);
  CHECK(!maybe_dest.IsEmpty());
  info.GetReturnValue().Set(maybe_dest.ToLocalChecked());
}

AWORKER_BINDING(InitBytes) {
  // atob / btoa
  immortal->SetFunctionProperty(exports, "atob", AToB);
  immortal->SetFunctionProperty(exports, "btoa", BToA);
  immortal->SetFunctionProperty(exports, "bufferEncodeHex", BufferEncodeHex);
  immortal->SetFunctionProperty(
      exports, "bufferEncodeBase64", BufferEncodeBase64);

  immortal->SetFunctionProperty(exports, "domainToASCII", DomainToASCII);
  immortal->SetFunctionProperty(exports, "toUSVString", ToUSVString);
}

AWORKER_EXTERNAL_REFERENCE(InitBytes) {
  registry->Register(AToB);
  registry->Register(BToA);
  registry->Register(BufferEncodeHex);
  registry->Register(BufferEncodeBase64);

  registry->Register(DomainToASCII);
  registry->Register(ToUSVString);
}

}  // namespace aworker

AWORKER_BINDING_REGISTER(bytes, aworker::InitBytes, aworker::InitBytes)
