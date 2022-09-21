#ifndef SRC_URL_URL_H_
#define SRC_URL_URL_H_

#include <string>
#include "immortal.h"

namespace aworker {

class UrlSearchParams {
 public:
  using ParamEntry = std::pair<std::string, std::string>;

  explicit UrlSearchParams(const std::string& query);
  explicit UrlSearchParams(const std::vector<ParamEntry>& params);

  std::string ToString() const;
  std::vector<ParamEntry>& params() { return params_; }

 private:
  void Append(const std::string& key, const std::string& value);
  std::vector<ParamEntry> params_;
};

// TODO(chengzhong.wcz): refactor to index-based storage
// https://source.chromium.org/chromium/chromium/src/+/main:url/third_party/mozilla/url_parse.h
class Url {
  void Parse(const std::string& url, const Url* base = nullptr);

 public:
  Url() : is_valid_(false) {}
  explicit Url(const std::string& url);
  Url(const std::string& url, const std::string& base);
  Url(const Url& other);
  void operator=(const Url& other);

  bool is_valid() const;

  std::string Origin() const;
  std::string Protocol() const;
  std::string Username() const;
  std::string Password() const;
  std::string Host() const;
  std::string Hostname() const;
  std::string Port() const;
  std::string Pathname() const;
  std::string Search() const;
  std::string Hash() const;

  std::string Href() const;

  void SetProtocol(const std::string& val);
  void SetUsername(const std::string& val);
  void SetPassword(const std::string& val);
  void SetHost(const std::string& val);
  void SetHostname(const std::string& val);
  void SetPort(const std::string& val);
  void SetPathname(const std::string& val);
  void SetSearch(const std::string& val);
  void SetHash(const std::string& val);

  bool MaybeSetHref(const std::string& val);

  static std::string UpdateProtocol(const std::string& protocol);
  static std::string UpdateUserInfo(const std::string& userinfo);
  static std::pair<std::string, std::string> UpdateHost(
      const std::string& protocol, const std::string& host);
  static std::string UpdateHostname(const std::string& protocol,
                                    const std::string& hostname);
  static std::string UpdatePort(const std::string& protocol,
                                const std::string& port);
  static std::string UpdatePathname(const std::string& protocol,
                                    const std::string& pathname);
  static std::string UpdateSearch(const std::string& protocol,
                                  const std::string& search);
  static std::string UpdateHash(const std::string& hash);

 private:
  bool is_valid_;
  std::string protocol_;
  std::string slashes_;
  std::string username_;
  std::string password_;
  std::string hostname_;
  std::string port_;
  std::string pathname_;
  std::string search_;
  std::string hash_;
};

}  // namespace aworker

#endif  // SRC_URL_URL_H_
