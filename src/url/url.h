#ifndef SRC_URL_URL_H_
#define SRC_URL_URL_H_

#include <string>
#include <utility>
#include <vector>

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

}  // namespace aworker

#endif  // SRC_URL_URL_H_
