#ifndef SRC_URL_URI_ENCODE_H_
#define SRC_URL_URI_ENCODE_H_

#include <stdlib.h>
#include "uri_encode-macros.data"

namespace aworker {
namespace uri_encode {

size_t Encode(const char* src,
              const size_t len,
              char* dst,
              bool space_to_plus = true);
size_t Decode(const char* src, const size_t len, char* dst);

}  // namespace uri_encode
}  // namespace aworker

#endif  // SRC_URL_URI_ENCODE_H_
