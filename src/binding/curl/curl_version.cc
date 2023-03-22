#include "binding/curl/curl_version.h"

#include <curl/curl.h>
#include "immortal.h"

namespace aworker {
namespace curl {
using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;

template <typename TValue>
void SetObjPropertyToNullOrValue(Immortal* immortal,
                                 Local<v8::Object> obj,
                                 const char* key,
                                 TValue value) {
  immortal->SetValueProperty(obj, key, value);
}

template <>
void SetObjPropertyToNullOrValue<int>(Immortal* immortal,
                                      Local<v8::Object> obj,
                                      const char* key,
                                      int value) {
  immortal->SetValueProperty(
      obj, key, v8::Int32::New(immortal->isolate(), value));
}

template <>
void SetObjPropertyToNullOrValue<unsigned int>(Immortal* immortal,
                                               Local<v8::Object> obj,
                                               const char* key,
                                               unsigned int value) {
  immortal->SetValueProperty(
      obj, key, v8::Int32::New(immortal->isolate(), value));
}

template <>
void SetObjPropertyToNullOrValue<const char*>(Immortal* immortal,
                                              Local<v8::Object> obj,
                                              const char* key,
                                              const char* value) {
  Isolate* isolate = immortal->isolate();
  if (value == nullptr) {
    immortal->SetValueProperty(obj, key, v8::Null(isolate));
  } else {
    immortal->SetValueProperty(
        obj, key, v8::String::NewFromUtf8(isolate, value).ToLocalChecked());
  }
}

const char* CurlVersions::get_version() {
  return curl_version();
}

const std::vector<CurlVersions::feature> CurlVersions::features = {
    {"AsynchDNS", CURL_VERSION_ASYNCHDNS},
    {"Debug", CURL_VERSION_DEBUG},
    {"TrackMemory", CURL_VERSION_CURLDEBUG},
    {"IDN", CURL_VERSION_IDN},
    {"IPv6", CURL_VERSION_IPV6},
    {"Largefile", CURL_VERSION_LARGEFILE},
    {"SSPI", CURL_VERSION_SSPI},
    {"GSS-API", CURL_VERSION_GSSAPI},
    {"Kerberos", CURL_VERSION_KERBEROS5},
    {"Kerberos", CURL_VERSION_KERBEROS4},
    {"SPNEGO", CURL_VERSION_SPNEGO},
    {"NTLM", CURL_VERSION_NTLM},
    {"NTLM_WB", CURL_VERSION_NTLM_WB},
    {"SSL", CURL_VERSION_SSL},
    {"libz", CURL_VERSION_LIBZ},
    {"brotli", CURL_VERSION_BROTLI},
    {"CharConv", CURL_VERSION_CONV},
    {"TLS-SRP", CURL_VERSION_TLSAUTH_SRP},
    {"HTTP2", CURL_VERSION_HTTP2},
    {"UnixSockets", CURL_VERSION_UNIX_SOCKETS},
    {"HTTPS-proxy", CURL_VERSION_HTTPS_PROXY},
    {"MultiSSL", CURL_VERSION_MULTI_SSL},
    {"PSL", CURL_VERSION_PSL},
    {"alt-svc", CURL_VERSION_ALTSVC},
};

const curl_version_info_data* CurlVersions::version_info =
    curl_version_info(CURLVERSION_NOW);

AWORKER_GETTER(CurlVersions::GetterProtocols) {
  Immortal* immortal = Immortal::GetCurrent(info);
  HandleScope scope(immortal->isolate());
  Local<Context> context = immortal->context();

  // const pointer to const char pointer
  const char* const* protocols = version_info->protocols;
  unsigned int i = 0;

  std::vector<const char*> vec;

  v8::Local<v8::Array> protocolsResult = v8::Array::New(immortal->isolate());

  for (i = 0; *(protocols + i); i++) {
    v8::Local<v8::String> protocol =
        v8::String::NewFromUtf8(immortal->isolate(), *(protocols + i))
            .ToLocalChecked();
    protocolsResult->Set(context, i, protocol).Check();
  }

  info.GetReturnValue().Set(protocolsResult);
}

// basically a copy of
// https://github.com/curl/curl/blob/05a131eb7740e/src/tool_help.c#L579
AWORKER_GETTER(CurlVersions::GetterFeatures) {
  Immortal* immortal = Immortal::GetCurrent(info);
  HandleScope scope(immortal->isolate());
  Local<Context> context = immortal->context();

  v8::Local<v8::Array> featuresResult = v8::Array::New(immortal->isolate());

  unsigned int currentFeature = 0;
  for (auto const& feat : CurlVersions::features) {
    if (version_info->features & feat.bitmask) {
      v8::Local<v8::String> featureString =
          v8::String::NewFromUtf8(immortal->isolate(), feat.name)
              .ToLocalChecked();
      featuresResult->Set(context, currentFeature++, featureString).Check();
    }
  }

  info.GetReturnValue().Set(featuresResult);
}

AWORKER_BINDING(CurlVersions::Initialize) {
  HandleScope scope(immortal->isolate());

  CHECK(version_info);

  v8::Local<v8::Object> obj = v8::Object::New(immortal->isolate());

  immortal->SetAccessor(obj, "protocols", GetterProtocols);
  immortal->SetAccessor(obj, "features", GetterFeatures);

  SetObjPropertyToNullOrValue(immortal, obj, "version", version_info->version);
  SetObjPropertyToNullOrValue(
      immortal, obj, "versionNumber", version_info->version_num);
  SetObjPropertyToNullOrValue(
      immortal, obj, "sslVersion", version_info->ssl_version);
  SetObjPropertyToNullOrValue(
      immortal, obj, "libzVersion", version_info->libz_version);
  SetObjPropertyToNullOrValue(immortal, obj, "aresVersion", version_info->ares);
  SetObjPropertyToNullOrValue(
      immortal, obj, "aresVersionNumber", version_info->ares_num);
  SetObjPropertyToNullOrValue(
      immortal, obj, "libidnVersion", version_info->libidn);
  SetObjPropertyToNullOrValue(
      immortal, obj, "iconvVersionNumber", version_info->iconv_ver_num);
  SetObjPropertyToNullOrValue(
      immortal, obj, "libsshVersion", version_info->libssh_version);
  SetObjPropertyToNullOrValue(
      immortal, obj, "brotliVersionNumber", version_info->brotli_ver_num);
  SetObjPropertyToNullOrValue(
      immortal, obj, "brotliVersion", version_info->brotli_version);
  SetObjPropertyToNullOrValue(
      immortal, obj, "nghttp2VersionNumber", version_info->nghttp2_ver_num);
  SetObjPropertyToNullOrValue(
      immortal, obj, "nghttp2Version", version_info->nghttp2_version);
  SetObjPropertyToNullOrValue(
      immortal, obj, "quicVersion", version_info->quic_version);

  immortal->SetValueProperty(exports, "versions", obj);
}

AWORKER_EXTERNAL_REFERENCE(CurlVersions::Initialize) {
  registry->Register(CurlVersions::GetterFeatures);
  registry->Register(CurlVersions::GetterProtocols);
}

}  // namespace curl
}  // namespace aworker
