#include "metadata.h"

extern "C" {
#include <zlib.h>
}

#include "ada.h"
#include "aworker_version.h"
#include "binding/curl/curl_version.h"
#include "uv.h"
#include "v8.h"

namespace aworker {

namespace per_process {
Metadata metadata;
}

Metadata::Metadata() : arch(AWORKER_ARCH), platform(AWORKER_PLATFORM) {
  ada = ADA_VERSION;
  aworker = AWORKER_VERSION_STRING;
  curl = curl::CurlVersions::get_version();
  v8 = v8::V8::GetVersion();
  uv = uv_version_string();
  zlib = ZLIB_VERSION;
}

}  // namespace aworker
