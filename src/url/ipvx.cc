#include "ipvx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "utils/resizable_buffer.h"

namespace aworker {
namespace ipvx {

CompressIPv6Result CompressIPv6(const char* in,
                                int in_len,
                                char* out,
                                int* out_len) {
  unsigned char buf[sizeof(struct in6_addr)];
  int ret = inet_pton(AF_INET6, in, buf);
  if (ret <= 0) {
    return CompressIPv6Result::kIPv6CompressFail;
  }

  if (inet_ntop(AF_INET6, buf, out, INET6_ADDRSTRLEN) == nullptr) {
    return CompressIPv6Result::kIPv6CompressFail;
  }

  *out_len = strlen(out);
  return CompressIPv6Result::kIPv6CompressOk;
}

TransformIPv4SegmentResult TransformIPv4SegmentFromHex(const char* seg,
                                                       unsigned int up_bound,
                                                       unsigned int* out) {
  uint64_t ret = 0;
  const char* ptr = seg;

  while (*ptr) {
    ret *= 16;
    if (*ptr >= '0' && *ptr <= '9') {
      ret += (*ptr - '0');
    } else if (*ptr >= 'A' && *ptr <= 'F') {
      ret += (*ptr - 'A' + 10);
    } else if (*ptr >= 'a' && *ptr <= 'f') {
      ret += (*ptr - 'a' + 10);
    } else {
      return TransformIPv4SegmentResult::kNotANumber;
    }

    if (ret > up_bound) return TransformIPv4SegmentResult::kGreaterThanUpBound;
    ptr++;
  }

  *out = ret;
  return TransformIPv4SegmentResult::kTransformOk;
}

TransformIPv4SegmentResult TransformIPv4SegmentFromOct(const char* seg,
                                                       unsigned int up_bound,
                                                       unsigned int* out) {
  uint64_t ret = 0;
  const char* ptr = seg;

  while (*ptr) {
    ret *= 8;
    if (*ptr >= '0' && *ptr <= '7') {
      ret += (*ptr - '0');
    } else {
      return TransformIPv4SegmentResult::kNotANumber;
    }

    if (ret > up_bound) return TransformIPv4SegmentResult::kGreaterThanUpBound;
    ptr++;
  }

  *out = ret;
  return TransformIPv4SegmentResult::kTransformOk;
}

TransformIPv4SegmentResult TransformIPv4SegmentFromDec(const char* seg,
                                                       unsigned int up_bound,
                                                       unsigned int* out) {
  uint64_t ret = 0;
  const char* ptr = seg;

  while (*ptr) {
    ret *= 10;
    if (*ptr >= '0' && *ptr <= '9') {
      ret += (*ptr - '0');
    } else {
      return TransformIPv4SegmentResult::kNotANumber;
    }

    if (ret > up_bound) return TransformIPv4SegmentResult::kGreaterThanUpBound;
    ptr++;
  }

  *out = ret;
  return TransformIPv4SegmentResult::kTransformOk;
}

TransformIPv4SegmentResult TransformIPv4SegmentToNumber(const char* segment,
                                                        unsigned int up_bound,
                                                        unsigned int* out) {
  TransformIPv4SegmentResult ret;
  int offset = 0;
  const char* ptr = segment;
  TransformIPv4SegmentFunction func = nullptr;

  if (*ptr != '0') {
    func = TransformIPv4SegmentFromDec;
  } else {
    switch (*(ptr + 1)) {
      case 'x':
      case 'X':
        offset = 2;
        func = TransformIPv4SegmentFromHex;
        break;

      default:
        offset = 1;
        func = TransformIPv4SegmentFromOct;
        break;
    }
  }

  ret = func(segment + offset, up_bound, out);
  if (ret < TransformIPv4SegmentResult::kTransformOk) {
    *out = 0;
  }

  return ret;
}

FormatIPv4Result FormatIPv4(const char* in,
                            int in_len,
                            char* out,
                            int* out_len) {
  // 由于 inet_aton 只能转换 IPv4，但不能检测出它不是一个 IP（而非非法 IP）。
  // 所以此处自行实现。

  *out_len = 0;
  memset(out, 0, sizeof(char) * (MAX_FORMATTED_IPV4_LEN + 1));
  if (!in_len || *in == '.' || *(in + in_len - 1) == '.') {
    return FormatIPv4Result::kFormatIPv4NotAnIPv4Address;
  }

  ResizableBuffer in_copy(in_len + 1);
  char* in_signed_buffer = static_cast<char*>(in_copy.buffer());
  memcpy(in_signed_buffer, in, sizeof(char) * (in_len));
  in_signed_buffer[in_len] = 0;

  char* ptr = in_signed_buffer + 1;
  char* in_segs[4] = {in_signed_buffer, nullptr, nullptr, nullptr};
  unsigned int ip_segments[4] = {0, 0, 0, 0};
  int segs_count = 1;

  while (*ptr != 0) {
    if (*(ptr - 1) == '.') {
      if (*ptr == '.' || segs_count >= 4) {
        return FormatIPv4Result::kFormatIPv4NotAnIPv4Address;
      }

      *(ptr - 1) = 0;
      in_segs[segs_count++] = ptr;
    }

    ptr++;
  }

  for (int i = 0; i < segs_count; i++) {
    ptr = in_segs[i];
    unsigned int up_bound = 255;
    TransformIPv4SegmentResult ret;
    if (i == 0 && segs_count == 1) {
      up_bound = 0xffffffff;
    }

    ret = TransformIPv4SegmentToNumber(in_segs[i], up_bound, ip_segments + i);
    switch (ret) {
      case TransformIPv4SegmentResult::kNotANumber:
        return FormatIPv4Result::kFormatIPv4NotAnIPv4Address;

      case TransformIPv4SegmentResult::kGreaterThanUpBound:
        return FormatIPv4Result::kFormatIPv4Fail;

      default:
        break;
    }
  }

  if (segs_count == 4) {
    snprintf(out,
             MAX_FORMATTED_IPV4_LEN + 1,
             "%d.%d.%d.%d",
             ip_segments[0],
             ip_segments[1],
             ip_segments[2],
             ip_segments[3]);
  } else {
    switch (segs_count) {
      case 1: {
        unsigned char bytes[4];
        bytes[0] = *ip_segments & 0xFF;
        bytes[1] = (*ip_segments >> 8) & 0xFF;
        bytes[2] = (*ip_segments >> 16) & 0xFF;
        bytes[3] = (*ip_segments >> 24) & 0xFF;
        snprintf(out,
                 MAX_FORMATTED_IPV4_LEN + 1,
                 "%d.%d.%d.%d",
                 bytes[3],
                 bytes[2],
                 bytes[1],
                 bytes[0]);
        break;
      }

      case 2: {
        snprintf(out,
                 MAX_FORMATTED_IPV4_LEN + 1,
                 "%d.0.0.%d",
                 ip_segments[0],
                 ip_segments[1]);
        break;
      }

      case 3: {
        snprintf(out,
                 MAX_FORMATTED_IPV4_LEN + 1,
                 "%d.%d.0.%d",
                 ip_segments[0],
                 ip_segments[1],
                 ip_segments[3]);
        break;
      }

      default: {
        return FormatIPv4Result::kFormatIPv4Fail;
      }
    }
  }

  *out_len = strlen(out);
  return FormatIPv4Result::kFormatIPv4Ok;
}

}  // namespace ipvx
}  // namespace aworker
