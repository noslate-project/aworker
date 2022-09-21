#include "uri_encode.h"

namespace aworker {
namespace uri_encode {

size_t Encode(const char* src,
              const size_t len,
              char* dst,
              bool space_to_plus) {
  size_t i = 0, j = 0;
  while (i < len) {
    const char octet = src[i++];
    if (octet == ' ' && space_to_plus) {
      dst[j++] = '+';
      continue;
    }

    const int32_t code =
        ((int32_t*)uri_encode_tbl)[(  // NOLINT:readability/casting
            unsigned char)octet];     // NOLINT:readability/casting
    if (code) {
      *((int32_t*)&dst[j]) = code;  // NOLINT:readability/casting
      j += 3;
    } else {
      dst[j++] = octet;
    }
  }
  dst[j] = '\0';
  return j;
}

size_t Decode(const char* src, const size_t len, char* dst) {
  size_t i = 0, j = 0;
  while (i < len) {
    int copy_char = 1;
    if (src[i] == '+') {
      dst[j] = ' ';
      i++;
      j++;
      continue;
    }

    if (src[i] == '%' && i + 2 < len) {
      const unsigned char v1 =
          hexval[(unsigned char)src[i + 1]];  // NOLINT:readability/casting
      const unsigned char v2 =
          hexval[(unsigned char)src[i + 2]];  // NOLINT:readability/casting

      /* skip invalid hex sequences */
      if ((v1 | v2) != 0xFF) {
        dst[j] = (v1 << 4) | v2;
        j++;
        i += 3;
        copy_char = 0;
      }
    }
    if (copy_char) {
      dst[j] = src[i];
      i++;
      j++;
    }
  }
  dst[j] = '\0';
  return j;
}

}  // namespace uri_encode
}  // namespace aworker
