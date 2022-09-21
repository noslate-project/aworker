#ifndef SRC_URL_IPVX_H_
#define SRC_URL_IPVX_H_

#define MAX_FORMATTED_IPV4_LEN (15)

namespace aworker {
namespace ipvx {

enum CompressIPv6Result { kIPv6CompressOk = 0, kIPv6CompressFail = -1 };
enum FormatIPv4Result {
  kFormatIPv4Ok = 0,
  kFormatIPv4Fail = -1,
  kFormatIPv4NotAnIPv4Address = -2
};
enum TransformIPv4SegmentResult {
  kTransformOk = 0,
  kNotANumber = -1,
  kGreaterThanUpBound = -2
};

CompressIPv6Result CompressIPv6(const char* in,
                                int in_len,
                                char* out,
                                int* out_len);
FormatIPv4Result FormatIPv4(const char* in,
                            int in_len,
                            char* out,
                            int* out_len);

typedef TransformIPv4SegmentResult (*TransformIPv4SegmentFunction)(
    const char* segment, unsigned int up_bound, unsigned int* out);

TransformIPv4SegmentResult TransformIPv4SegmentToNumber(const char* segment,
                                                        unsigned int up_bound,
                                                        unsigned int* out);
TransformIPv4SegmentResult TransformIPv4SegmentFromOct(const char* seg,
                                                       unsigned int up_bound,
                                                       unsigned int* out);
TransformIPv4SegmentResult TransformIPv4SegmentFromDec(const char* seg,
                                                       unsigned int up_bound,
                                                       unsigned int* out);
TransformIPv4SegmentResult TransformIPv4SegmentFromHex(const char* seg,
                                                       unsigned int up_bound,
                                                       unsigned int* out);

}  // namespace ipvx
}  // namespace aworker

#endif  // SRC_URL_IPVX_H_
