#ifndef SRC_BINDING_INTERNAL_AWORKER_CRYPTO_H_
#define SRC_BINDING_INTERNAL_AWORKER_CRYPTO_H_
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include "aworker_binding.h"
#include "base_object.h"
#include "util.h"
#include "utils/resizable_buffer.h"
#include "v8.h"

namespace aworker {

using EVPMDPointer = DeleteFnPtr<EVP_MD_CTX, EVP_MD_CTX_free>;
using HMACCtxPointer = DeleteFnPtr<HMAC_CTX, HMAC_CTX_free>;

class Hash final : public BaseObject {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static void Initialize(Immortal* immortal, v8::Local<v8::Object> target);
  static void Initialize(ExternalReferenceRegistry* registry);

  bool HashInit(const EVP_MD* md, v8::Maybe<unsigned int> xof_md_len);
  bool HashUpdate(const char* data, size_t len);

  static void GetHashes(const v8::FunctionCallbackInfo<v8::Value>& args);

 protected:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HashUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HashDigest(const v8::FunctionCallbackInfo<v8::Value>& args);

  Hash(Immortal* immortal, v8::Local<v8::Object> wrap);

 private:
  EVPMDPointer _mdctx{};
  bool _has_md;
  unsigned int _md_len;
};

class Hmac final : public BaseObject {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static void Initialize(Immortal* immortal, v8::Local<v8::Object> target);
  static void Initialize(ExternalReferenceRegistry* registry);

 protected:
  void HmacInit(const char* hash_type, const char* key, int key_len);
  bool HmacUpdate(const char* data, size_t len);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HmacUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HmacDigest(const v8::FunctionCallbackInfo<v8::Value>& args);

  Hmac(Immortal* immortal, v8::Local<v8::Object> wrap);

  static void Sign(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  HMACCtxPointer _ctx;
};

void ThrowCryptoError(Immortal* immortal,
                      unsigned long err,  // NOLINT(runtime/int)
                      const char* message = nullptr);

template <typename T>
inline T* MallocOpenSSL(size_t byte_length) {
  void* mem = OPENSSL_malloc(MultiplyWithOverflowCheck(byte_length, sizeof(T)));
  CHECK_IMPLIES(mem == nullptr, byte_length == 0);
  return static_cast<T*>(mem);
}

}  // namespace aworker

#endif  // SRC_BINDING_INTERNAL_AWORKER_CRYPTO_H_
