#include "url/url.h"

#include <cwalk.h>
#include <map>
#include "aworker_binding.h"
#include "binding/core/text.h"
#include "url/uri_encode.h"
#include "util.h"
#include "utils/resizable_buffer.h"

namespace aworker {

UrlSearchParams::UrlSearchParams(const std::vector<ParamEntry>& params)
    : params_(params) {}

UrlSearchParams::UrlSearchParams(const std::string& search) {
  int32_t offset = search.length() >= 1 && search[0] == '?' ? 1 : 0;
  int32_t len = search.length() - offset;
  const char* init_ptr = search.c_str() + offset;
  const char* ptr = init_ptr;

  const char* key = nullptr;
  const char* value = nullptr;
  int32_t key_len = 0;
  int32_t value_len = 0;

  enum ParseStringURLSearchParamsState { KEY = 0, VALUE, EQUAL, AND };

  ParseStringURLSearchParamsState state = ParseStringURLSearchParamsState::AND;
  for (int32_t i = 0; i < len; i++, ptr++) {
    switch (*ptr) {
      case '&': {
        switch (state) {
            // &&
          case ParseStringURLSearchParamsState::AND:
            break;

            // =&
          case ParseStringURLSearchParamsState::EQUAL:
            // foo&
          case ParseStringURLSearchParamsState::KEY:
            // foo=bar&
          case ParseStringURLSearchParamsState::VALUE:
            Append(std::string(key, key_len), std::string(value, value_len));
            key_len = value_len = 0;
            key = value = nullptr;
            state = ParseStringURLSearchParamsState::AND;
            break;

          default:
            UNREACHABLE();
        }

        break;
      }

      case '=': {
        switch (state) {
            // &=<...>
          case ParseStringURLSearchParamsState::AND:
            // foo=<...>
          case ParseStringURLSearchParamsState::KEY:
            state = ParseStringURLSearchParamsState::EQUAL;
            break;

            // foo==
          case ParseStringURLSearchParamsState::EQUAL:
            value = ptr;
            value_len = 1;
            state = ParseStringURLSearchParamsState::VALUE;
            break;

            // foo=bar=
          case ParseStringURLSearchParamsState::VALUE:
            value_len++;
            break;

          default:
            UNREACHABLE();
        }

        break;
      }

      default: {
        switch (state) {
            // &foo
          case ParseStringURLSearchParamsState::AND:
            key = ptr;
            key_len = 1;
            state = ParseStringURLSearchParamsState::KEY;
            break;

            // =bar
          case ParseStringURLSearchParamsState::EQUAL:
            value = ptr;
            value_len = 1;
            state = ParseStringURLSearchParamsState::VALUE;
            break;

            // foo...
          case ParseStringURLSearchParamsState::KEY:
            key_len++;
            break;

            // =bar...
          case ParseStringURLSearchParamsState::VALUE:
            value_len++;
            break;

          default:
            UNREACHABLE();
        }

        break;
      }
    }
  }

  // the last tail
  if (key_len || value_len || state == ParseStringURLSearchParamsState::EQUAL) {
    Append(std::string(key, key_len), std::string(value, value_len));
  }
}

void UrlSearchParams::Append(const std::string& key, const std::string& value) {
  ResizableBuffer escaped_key(key.length() + 1);
  ResizableBuffer escaped_value(value.length() + 1);
  int32_t escaped_key_len =
      key.length()
          ? uri_encode::Decode(key.c_str(),
                               key.length(),
                               static_cast<char*>(escaped_key.buffer()))
          : 0;
  int32_t escaped_value_len =
      value.length()
          ? uri_encode::Decode(value.c_str(),
                               value.length(),
                               static_cast<char*>(escaped_value.buffer()))
          : 0;
  params_.emplace_back(
      std::string(static_cast<char*>(escaped_key.buffer()), escaped_key_len),
      std::string(static_cast<char*>(escaped_value.buffer()),
                  escaped_value_len));
}

std::string UrlSearchParams::ToString() const {
  std::string ret = "";
  if (params_.size()) {
    size_t max_len = 0;
    size_t encoded_len = 0;
    ResizableBuffer buff(0);

    int i = 0;
    for (auto it = params_.begin(); it != params_.end(); it++, i++) {
      if (i) ret += "&";

      const std::string& key = it->first;
      const std::string& value = it->second;

      if (key.length()) {
        max_len = key.length() * 3 + 1;
        if (max_len > static_cast<size_t>(buff.byte_length()))
          buff.Realloc(max_len);
        encoded_len = uri_encode::Encode(
            key.c_str(), key.length(), static_cast<char*>(buff.buffer()), true);
        ret += std::string(static_cast<char*>(buff.buffer()), encoded_len);
      }

      ret += "=";

      if (value.length()) {
        max_len = value.length() * 3 + 1;
        if (max_len > static_cast<size_t>(buff.byte_length()))
          buff.Realloc(max_len);
        encoded_len = uri_encode::Encode(value.c_str(),
                                         value.length(),
                                         static_cast<char*>(buff.buffer()),
                                         true);
        ret += std::string(static_cast<char*>(buff.buffer()), encoded_len);
      }
    }
  }

  return ret;
}

}  // namespace aworker
