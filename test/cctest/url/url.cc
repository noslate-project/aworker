#include <gtest/gtest.h>
#include <string>

#include "url/url.h"

namespace aworker {

TEST(Url, ParseHttpUrl) {
  std::string str = "http:///example.com:80/foo?bar=123#hash";
  Url url(str);

  EXPECT_EQ(url.Origin(), "http://example.com");
  EXPECT_EQ(url.Protocol(), "http:");
  EXPECT_EQ(url.Username(), "");
  EXPECT_EQ(url.Password(), "");
  EXPECT_EQ(url.Host(), "example.com");
  EXPECT_EQ(url.Hostname(), "example.com");
  EXPECT_EQ(url.Port(), "");
  EXPECT_EQ(url.Pathname(), "/foo");
  EXPECT_EQ(url.Search(), "?bar=123");
  EXPECT_EQ(url.Hash(), "#hash");
  EXPECT_EQ(url.Href(), "http://example.com/foo?bar=123#hash");
}

TEST(Url, ParseHttpUrlPort) {
  std::string str = "http:///example.com:12345/foo?bar=123#hash";
  Url url(str);

  EXPECT_EQ(url.Origin(), "http://example.com:12345");
  EXPECT_EQ(url.Protocol(), "http:");
  EXPECT_EQ(url.Username(), "");
  EXPECT_EQ(url.Password(), "");
  EXPECT_EQ(url.Host(), "example.com:12345");
  EXPECT_EQ(url.Hostname(), "example.com");
  EXPECT_EQ(url.Port(), "12345");
  EXPECT_EQ(url.Pathname(), "/foo");
  EXPECT_EQ(url.Search(), "?bar=123");
  EXPECT_EQ(url.Hash(), "#hash");
  EXPECT_EQ(url.Href(), "http://example.com:12345/foo?bar=123#hash");
}

TEST(Url, HttpUrlHrefTrailingSlash) {
  std::string str = "http:///example.com:12345";
  Url url(str);

  EXPECT_EQ(url.Origin(), "http://example.com:12345");
  EXPECT_EQ(url.Protocol(), "http:");
  EXPECT_EQ(url.Username(), "");
  EXPECT_EQ(url.Password(), "");
  EXPECT_EQ(url.Host(), "example.com:12345");
  EXPECT_EQ(url.Hostname(), "example.com");
  EXPECT_EQ(url.Port(), "12345");
  EXPECT_EQ(url.Pathname(), "/");
  EXPECT_EQ(url.Search(), "");
  EXPECT_EQ(url.Hash(), "");
  EXPECT_EQ(url.Href(), "http://example.com:12345/");
}

TEST(Url, SetPortWithControlChar) {
  std::string str = "http:///example.com:12345";
  Url url(str);

  url.SetPort("\u00008000");
  EXPECT_EQ(url.Port(), "12345");
  url.SetPort("90\u000000");
  EXPECT_EQ(url.Port(), "90");

  url.SetPort("\u000A8000");
  EXPECT_EQ(url.Port(), "8000");
  url.SetPort("90\u000A00");
  EXPECT_EQ(url.Port(), "9000");
}

}  // namespace aworker
