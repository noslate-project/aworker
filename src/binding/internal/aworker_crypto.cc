#include "aworker_crypto.h"
#include "error_handling.h"

namespace aworker {

using std::shared_ptr;
using v8::ArrayBuffer;
using v8::BackingStore;
using v8::Exception;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Nothing;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;
using v8::Uint32;
using v8::Value;

struct CryptoErrorVector : public std::vector<std::string> {
  inline void Capture() {
    clear();
    while (auto err = ERR_get_error()) {
      char buf[256];
      ERR_error_string_n(err, buf, sizeof(buf));
      push_back(buf);
    }
    std::reverse(begin(), end());
  }

  inline MaybeLocal<Value> ToException(
      Immortal* immortal,
      Local<String> exception_string = Local<String>()) const {
    if (exception_string.IsEmpty()) {
      CryptoErrorVector copy(*this);
      if (copy.empty()) copy.push_back("no error");  // But possibly a bug...
      // Use last element as the error message, everything else goes
      // into the .opensslErrorStack property on the exception object.
      auto exception_string = String::NewFromUtf8(immortal->isolate(),
                                                  copy.back().data(),
                                                  NewStringType::kNormal,
                                                  copy.back().size())
                                  .ToLocalChecked();
      copy.pop_back();
      return copy.ToException(immortal, exception_string);
    }

    Local<Value> exception_v = Exception::Error(exception_string);
    CHECK(!exception_v.IsEmpty());

    // TODO(chengzhong.wcz): convert to an error string array.
    // if (!empty()) {
    //   CHECK(exception_v->IsObject());
    //   Local<Object> exception = exception_v.As<Object>();
    //   Maybe<bool> ok = exception->Set(immortal->context(),
    //                  immortal->openssl_error_stack_string(),
    //                  ToV8Value(immortal->context(), *this).ToLocalChecked());
    //   if (ok.IsNothing())
    //     return MaybeLocal<Value>();
    // }

    return exception_v;
  }
};

const WrapperTypeInfo Hash::wrapper_type_info_{
    "hash",
};

Hash::Hash(Immortal* immortal, Local<Object> wrap)
    : BaseObject(immortal, wrap), _mdctx(nullptr), _has_md(false) {
  MakeWeak();
}

void Hash::Initialize(Immortal* immortal, Local<Object> target) {
  Local<FunctionTemplate> t = FunctionTemplate::New(immortal->isolate(), New);

  // Const Variable of InternalFieldCount
  t->InstanceTemplate()->SetInternalFieldCount(1);

  Local<ObjectTemplate> prototype_template = t->PrototypeTemplate();
  immortal->SetFunctionProperty(prototype_template, "update", HashUpdate);
  immortal->SetFunctionProperty(prototype_template, "digest", HashDigest);

  target
      ->Set(immortal->context(),
            OneByteString(immortal->isolate(), "Hash"),
            t->GetFunction(immortal->context()).ToLocalChecked())
      .Check();
}

void Hash::Initialize(ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(HashUpdate);
  registry->Register(HashDigest);
}

void Hash::New(const FunctionCallbackInfo<Value>& args) {
  Immortal* immortal = Immortal::GetCurrent(args);

  const Hash* orig = nullptr;
  const EVP_MD* md = nullptr;

  if (args[0]->IsObject()) {
    orig = Unwrap<Hash>(args.Holder());
    CHECK_NE(orig, nullptr);
    md = EVP_MD_CTX_md(orig->_mdctx.get());
  } else {
    const String::Utf8Value hash_type(immortal->isolate(), args[0]);
    md = EVP_get_digestbyname(*hash_type);
  }

  Maybe<unsigned int> xof_md_len = Nothing<unsigned int>();
  if (!args[1]->IsUndefined()) {
    CHECK(args[1]->IsUint32());
    xof_md_len = Just<unsigned int>(args[1].As<Uint32>()->Value());
  }

  Hash* hash = new Hash(immortal, args.This());
  if (md == nullptr || !hash->HashInit(md, xof_md_len)) {
    return ThrowCryptoError(
        immortal, ERR_get_error(), "Digest method not supported");
  }

  if (orig != nullptr &&
      0 >= EVP_MD_CTX_copy(hash->_mdctx.get(), orig->_mdctx.get())) {
    return ThrowCryptoError(immortal, ERR_get_error(), "Digest copy error");
  }
}

bool Hash::HashInit(const EVP_MD* md, Maybe<unsigned int> xof_md_len) {
  _mdctx.reset(EVP_MD_CTX_new());
  if (!_mdctx || EVP_DigestInit_ex(_mdctx.get(), md, nullptr) <= 0) {
    _mdctx.reset();
    return false;
  }

  _md_len = EVP_MD_size(md);
  if (xof_md_len.IsJust() && xof_md_len.FromJust() != _md_len) {
    // This is a little hack to cause createHash to fail when an incorrect
    // hashSize option was passed for a non-XOF hash function.
    if ((EVP_MD_flags(md) & EVP_MD_FLAG_XOF) == 0) {
      EVPerr(EVP_F_EVP_DIGESTFINALXOF, EVP_R_NOT_XOF_OR_INVALID_LENGTH);
      return false;
    }
    _md_len = xof_md_len.FromJust();
  }

  return true;
}

bool Hash::HashUpdate(const char* data, size_t len) {
  if (!_mdctx) return false;
  EVP_DigestUpdate(_mdctx.get(), data, len);
  return true;
}

void Hash::HashUpdate(const FunctionCallbackInfo<Value>& args) {
  Hash* hash = Unwrap<Hash>(args.Holder());
  CHECK_NE(hash, nullptr);

  CHECK(args[0]->IsArrayBuffer());
  Local<ArrayBuffer> ab = args[0].As<ArrayBuffer>();
  std::shared_ptr<BackingStore> bs = ab->GetBackingStore();
  CHECK(args[1]->IsUint32());
  uint32_t offset = args[1].As<Uint32>()->Value();
  CHECK(args[2]->IsUint32());
  uint32_t byte_length = args[2].As<Uint32>()->Value();
  CHECK(offset + byte_length <= bs->ByteLength());

  bool r = hash->HashUpdate(static_cast<const char*>(bs->Data()) + offset,
                            byte_length);
  args.GetReturnValue().Set(r);
}

void Hash::HashDigest(const FunctionCallbackInfo<Value>& args) {
  Immortal* immortal = Immortal::GetCurrent(args);

  Hash* hash = Unwrap<Hash>(args.Holder());
  CHECK_NE(hash, nullptr);

  unsigned char* md_value;

  md_value = MallocOpenSSL<unsigned char>(hash->_md_len);
  size_t default_len = EVP_MD_CTX_size(hash->_mdctx.get());
  int ret;
  if (hash->_md_len == default_len) {
    ret = EVP_DigestFinal_ex(hash->_mdctx.get(), md_value, &hash->_md_len);
  } else {
    ret = EVP_DigestFinalXOF(hash->_mdctx.get(), md_value, hash->_md_len);
  }

  if (ret != 1) {
    OPENSSL_free(md_value);
    md_value = nullptr;
    return ThrowCryptoError(immortal, ERR_get_error());
  }

  Local<ArrayBuffer> ab = ArrayBuffer::New(
      immortal->isolate(),
      ArrayBuffer::NewBackingStore(
          md_value,
          hash->_md_len,
          [](void* buffer, size_t length, void* info) { OPENSSL_free(buffer); },
          nullptr));
  args.GetReturnValue().Set(ab);
}

const WrapperTypeInfo Hmac::wrapper_type_info_{
    "hmac",
};

Hmac::Hmac(Immortal* immortal, Local<Object> wrap)
    : BaseObject(immortal, wrap), _ctx(nullptr) {
  MakeWeak();
}

void Hmac::Initialize(Immortal* immortal, Local<Object> target) {
  Local<FunctionTemplate> t = FunctionTemplate::New(immortal->isolate(), New);

  // TODO(chengzhong.wcz): Const Variable of InternalFieldCount
  t->InstanceTemplate()->SetInternalFieldCount(1);

  auto prototype_template = t->PrototypeTemplate();
  immortal->SetFunctionProperty(prototype_template, "update", HmacUpdate);
  immortal->SetFunctionProperty(prototype_template, "digest", HmacDigest);

  target
      ->Set(immortal->context(),
            OneByteString(immortal->isolate(), "Hmac"),
            t->GetFunction(immortal->context()).ToLocalChecked())
      .Check();
}

void Hmac::Initialize(ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(HmacUpdate);
  registry->Register(HmacDigest);
}

void Hmac::New(const FunctionCallbackInfo<Value>& args) {
  Immortal* immortal = Immortal::GetCurrent(args);
  Hmac* hmac = new Hmac(immortal, args.This());

  CHECK(args[0]->IsString());
  const String::Utf8Value hash_type(immortal->isolate(), args[0]);
  CHECK(args[1]->IsArrayBuffer());
  Local<ArrayBuffer> ab = args[1].As<ArrayBuffer>();
  std::shared_ptr<v8::BackingStore> bs = ab->GetBackingStore();
  CHECK(args[2]->IsUint32());
  uint32_t offset = args[2].As<Uint32>()->Value();
  CHECK(args[3]->IsUint32());
  uint32_t byte_length = args[3].As<Uint32>()->Value();
  CHECK(offset + byte_length <= bs->ByteLength());

  hmac->HmacInit(
      *hash_type, static_cast<const char*>(bs->Data()) + offset, byte_length);
}

void Hmac::HmacInit(const char* hash_type, const char* key, int key_len) {
  HandleScope scope(immortal()->isolate());

  const EVP_MD* md = EVP_get_digestbyname(hash_type);
  if (md == nullptr) {
    return ThrowException(immortal()->isolate(), "Unknown message digest");
  }
  if (key_len == 0) {
    key = "";
  }
  _ctx.reset(HMAC_CTX_new());
  if (!_ctx || !HMAC_Init_ex(_ctx.get(), key, key_len, md, nullptr)) {
    _ctx.reset();
    return ThrowCryptoError(immortal(), ERR_get_error());
  }
}

bool Hmac::HmacUpdate(const char* data, size_t len) {
  if (!_ctx) return false;
  int r = HMAC_Update(
      _ctx.get(), reinterpret_cast<const unsigned char*>(data), len);
  return r == 1;
}

void Hmac::HmacUpdate(const FunctionCallbackInfo<Value>& args) {
  Hmac* hmac = Unwrap<Hmac>(args.Holder());
  CHECK_NE(hmac, nullptr);

  CHECK(args[0]->IsArrayBuffer());
  Local<ArrayBuffer> ab = args[0].As<ArrayBuffer>();
  std::shared_ptr<v8::BackingStore> bs = ab->GetBackingStore();
  CHECK(args[1]->IsUint32());
  uint32_t offset = args[1].As<Uint32>()->Value();
  CHECK(args[2]->IsUint32());
  uint32_t byte_length = args[2].As<Uint32>()->Value();
  CHECK(offset + byte_length <= bs->ByteLength());

  bool r = hmac->HmacUpdate(static_cast<const char*>(bs->Data()) + offset,
                            byte_length);
  args.GetReturnValue().Set(r);
}

void Hmac::HmacDigest(const FunctionCallbackInfo<Value>& args) {
  Immortal* immortal = Immortal::GetCurrent(args);
  Hmac* hmac = Unwrap<Hmac>(args.Holder());
  CHECK_NE(hmac, nullptr);

  unsigned char* md_value = MallocOpenSSL<unsigned char>(EVP_MAX_MD_SIZE);
  unsigned int md_len = 0;

  if (hmac->_ctx) {
    HMAC_Final(hmac->_ctx.get(), md_value, &md_len);
    hmac->_ctx.reset();
  }

  Local<ArrayBuffer> ab = ArrayBuffer::New(
      immortal->isolate(),
      ArrayBuffer::NewBackingStore(
          md_value,
          md_len,
          [](void* buffer, size_t length, void* info) { OPENSSL_free(buffer); },
          nullptr));
  args.GetReturnValue().Set(ab);
}

void ThrowCryptoError(Immortal* immortal,
                      unsigned long err,  // NOLINT(runtime/int)
                      // Default, only used if there is no SSL `err` which can
                      // be used to create a long-style message string.
                      const char* message) {
  char message_buffer[128] = {0};
  if (err != 0 || message == nullptr) {
    ERR_error_string_n(err, message_buffer, sizeof(message_buffer));
    message = message_buffer;
  }
  HandleScope scope(immortal->isolate());
  Local<String> exception_string =
      String::NewFromUtf8(immortal->isolate(), message).ToLocalChecked();
  CryptoErrorVector errors;
  errors.Capture();
  Local<Value> exception;
  if (!errors.ToException(immortal, exception_string).ToLocal(&exception))
    return;
  Local<Object> obj;
  if (!exception->ToObject(immortal->context()).ToLocal(&obj)) return;
  immortal->isolate()->ThrowException(exception);
}

AWORKER_METHOD(RandomBytes) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  int32_t size = info[0].As<Int32>()->Value();
  if (size == 0) {
    // Should not alloc backing store when the byte_length is 0.
    Local<ArrayBuffer> array_buffer = ArrayBuffer::New(isolate, size);
    info.GetReturnValue().Set(array_buffer);
    return;
  }
  ResizableBuffer* buff = new ResizableBuffer();
  buff->Realloc(size);

  RAND_bytes(static_cast<unsigned char*>(**buff), size);

  shared_ptr<v8::BackingStore> backing_store = ArrayBuffer::NewBackingStore(
      **buff,
      size,
      [](void* data, size_t length, void* deleter_data) {
        delete static_cast<ResizableBuffer*>(deleter_data);
      },
      buff);
  Local<ArrayBuffer> array_buffer = ArrayBuffer::New(isolate, backing_store);
  info.GetReturnValue().Set(array_buffer);
}

AWORKER_BINDING(InitCrypto) {
  Hash::Initialize(immortal, exports);
  Hmac::Initialize(immortal, exports);

  immortal->SetFunctionProperty(exports, "randomBytes", RandomBytes);
}

AWORKER_EXTERNAL_REFERENCE(InitCrypto) {
  Hash::Initialize(registry);
  Hmac::Initialize(registry);
  registry->Register(RandomBytes);
}

}  // namespace aworker

AWORKER_BINDING_REGISTER(crypto, aworker::InitCrypto, aworker::InitCrypto)
