#include "url/ipvx.h"
#include <gtest/gtest.h>

namespace aworker {
namespace ipvx {

TEST(ipvx, compress_ipv6) {
  char tests[][1024] = {"xxx:xxx:xxx",
                        "aAa:000:090",
                        "000:000:000:000:000:12321:12:00:00",
                        "ABCD:0ABC:00AB:000A:0000:0000:0000:1000",
                        "abcd:0000:0000",
                        "999:0000:0000",
                        "0000:0000",
                        ""};
  int rets[] = {-1, -1, -1, 0, -1, -1, -1};
  char tests_output[][1024] = {"", "", "", "abcd:abc:ab:a::1000", "", "", ""};

  char out[1024];
  int out_len;
  int ret;

  for (int i = 0; tests[i][0] != 0; i++) {
    memset(out, 0, sizeof(out));
    ret = CompressIPv6(tests[i], strlen(tests[i]), out, &out_len);
    EXPECT_EQ(ret, rets[i]);
    EXPECT_STREQ(out, tests_output[i]);
  }
}

TEST(ipvx, format_ipv4) {
  char tests[][1024] = {"111",
                        "111.222",
                        "111.222.333",
                        "111.222.333.444",
                        ".",
                        "sdflkj.sadf..",
                        "111.222.",
                        "213kjsfad.dfs",
                        "123",
                        "0x12312312",
                        "012312312312",
                        "0123.0123.0111111",
                        "0123.0123.0111.111",
                        "0x01",
                        ""};
  int rets[] = {0, 0, -1, -1, -2, -2, -2, -2, 0, 0, 0, -1, 0, 0};
  char tests_output[][1024] = {"0.0.0.111",
                               "111.0.0.222",
                               "",
                               "",
                               "",
                               "",
                               "",
                               "",
                               "0.0.0.123",
                               "18.49.35.18",
                               "83.41.148.202",
                               "",
                               "83.83.73.111",
                               "0.0.0.1"};
  char out[1024];
  int out_len;
  int ret;

  for (int i = 0; tests[i][0] != 0; i++) {
    memset(out, 0, sizeof(out));
    ret = FormatIPv4(tests[i], strlen(tests[i]), out, &out_len);
    EXPECT_EQ(ret, rets[i]);
    EXPECT_STREQ(out, tests_output[i]);
  }
}

}  // namespace ipvx
}  // namespace aworker
