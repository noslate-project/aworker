#ifndef SRC_BINDING_CURL_CURL_VERSION_H_
#define SRC_BINDING_CURL_CURL_VERSION_H_

#include <curl/curl.h>
#include "aworker_binding.h"

namespace aworker {
namespace curl {

class CurlVersions {
 public:
  static AWORKER_BINDING(Initialize);
  static AWORKER_EXTERNAL_REFERENCE(Initialize);

  static AWORKER_GETTER(GetterProtocols);
  static AWORKER_GETTER(GetterFeatures);

 private:
  CurlVersions();
  ~CurlVersions();

  CurlVersions(const CurlVersions& that);
  CurlVersions& operator=(const CurlVersions& that);

  struct feature {
    const char* name;
    int bitmask;
  };

  static const std::vector<feature> features;
  static const curl_version_info_data* version_info;
};

}  // namespace curl
}  // namespace aworker

#endif  // SRC_BINDING_CURL_CURL_VERSION_H_
