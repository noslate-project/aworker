#include "url/url.h"

#include <cwalk.h>
#include <map>
#include "aworker_binding.h"
#include "binding/core/text.h"
#include "url/ipvx.h"
#include "url/uri_encode.h"
#include "util.h"
#include "utils/resizable_buffer.h"
#include "utils/scope.h"

namespace aworker {
namespace {

struct URLPartsTemporaryCharPtr {
  const char* start;
  int32_t length;
};

constexpr const char* kSpecialProtocols[] = {
    "ftp:", "file:", "http:", "https:", "ws:", "wss:"};

// https://url.spec.whatwg.org/#special-scheme
const std::map<std::string, std::string> kSpecialProtocolPorts = {
    {"ftp:", "21"},
    {"file:", ""},
    {"http:", "80"},
    {"https:", "443"},
    {"ws:", "80"},
    {"wss", "443"}};

constexpr const char* kSlash = "/";

bool IsSlash(const char* ptr) {
  return *ptr == '/' || *ptr == '\\';
}

bool IsDoubleSlash(const char* ptr) {
  return ((*ptr == '/' || *ptr == '\\') &&
          (*(ptr + 1) == '/' || *(ptr + 1) == '\\'));
}

bool CharInC0ControlSet(unsigned char c) {
  return c <= 0x1f /* || c > 0x7e */;
}

#define V(value) (c == value) ||

#define SEARCH_SET                                                             \
  V(0x20)                                                                      \
  V(0x22)                                                                      \
  V(0x23)                                                                      \
  V(0x3c)                                                                      \
  V(0x3e)

#define FRAGMENT_SET                                                           \
  V(0x20)                                                                      \
  V(0x22)                                                                      \
  V(0x3c)                                                                      \
  V(0x3e)                                                                      \
  V(0x60)

#define PATH_SET                                                               \
  V(0x23)                                                                      \
  V(0x3f)                                                                      \
  V(0x7b)                                                                      \
  V(0x7d)

#define USER_INFO_SET                                                          \
  V(0x27) /* "\u0027" ("'") seemingly isn't in the spec, but matches Chrome */ \
          /* and Firefox. */                                                   \
  V(0x2f)                                                                      \
  V(0x3a)                                                                      \
  V(0x3b)                                                                      \
  V(0x3d)                                                                      \
  V(0x40)                                                                      \
  V(0x5b)                                                                      \
  V(0x5c)                                                                      \
  V(0x5d)                                                                      \
  V(0x5e)                                                                      \
  V(0x7c)

#define FORBIDDEN_IN_HOST                                                      \
  V(0x00)                                                                      \
  V(0x09)                                                                      \
  V(0x0a)                                                                      \
  V(0x0d)                                                                      \
  V(0x20)                                                                      \
  V(0x23)                                                                      \
  V(0x25)                                                                      \
  V(0x2f)                                                                      \
  V(0x3a)                                                                      \
  V(0x3c)                                                                      \
  V(0x3e)                                                                      \
  V(0x3f)                                                                      \
  V(0x40)                                                                      \
  V(0x5b)                                                                      \
  V(0x5c)                                                                      \
  V(0x5d)                                                                      \
  V(0x5e)

#define FORM_URLENCODED_SET                                                    \
  V(0x21)                                                                      \
  V(0x24)                                                                      \
  V(0x25)                                                                      \
  V(0x26)                                                                      \
  V(0x27)                                                                      \
  V(0x28)                                                                      \
  V(0x29)                                                                      \
  V(0x2b)                                                                      \
  V(0x2c)                                                                      \
  V(0x7e)

#define MATCH_CHARACTERS($) ($ false)

bool CharInSearchSet(unsigned char c, bool is_special) {
  return CharInC0ControlSet(c) || MATCH_CHARACTERS(SEARCH_SET) ||
         (is_special && c == 0x27) || c > 0x7e;
}

bool CharInFragmentSet(unsigned char c) {
  return CharInC0ControlSet(c) || MATCH_CHARACTERS(FRAGMENT_SET);
}

bool CharInPathSet(unsigned char c) {
  return CharInFragmentSet(c) || MATCH_CHARACTERS(PATH_SET);
}

bool CharInUserInfoSet(unsigned char c) {
  return CharInPathSet(c) || MATCH_CHARACTERS(USER_INFO_SET);
}

bool CharIsForbiddenInHost(unsigned char c) {
  return MATCH_CHARACTERS(FORBIDDEN_IN_HOST);
}

#undef V
#undef MATCH_CHARACTERS

#define URL_PARTS_CHARACTERS_ENCODE(c)                                         \
  (uri_encode::uri_encode_tbl[c * sizeof(int32_t)])
#define URL_PARTS_CHARACTERS_ENCODE_WITH_SPACE_TO_PLUS(c)                      \
  (c == ' ' ? *_plus : uri_encode::uri_encode_tbl[c * sizeof(int32_t)])

constexpr char _plus[] = {' ', 0};
std::string EncodeUserInfo(const std::string& orig) {
  std::string ret = "";
  unsigned char c;
  for (auto it = orig.begin(); it != orig.end(); it++) {
    c = *it;
    if (CharInUserInfoSet(c)) {
      ret += &URL_PARTS_CHARACTERS_ENCODE(c);
    } else {
      ret += c;
    }
  }
  return ret;
}

std::string EncodeQuery(const std::string& orig, bool is_special) {
  std::string ret = "";
  unsigned char c;
  for (auto it = orig.begin(); it != orig.end(); it++) {
    c = *it;
    if (CharInSearchSet(c, is_special)) {
      ret += &URL_PARTS_CHARACTERS_ENCODE_WITH_SPACE_TO_PLUS(c);
    } else {
      ret += c;
    }
  }
  return ret;
}

std::string EncodeHash(const std::string& orig) {
  std::string ret = "";
  unsigned char c;
  for (auto it = orig.begin(); it != orig.end(); it++) {
    c = *it;
    if (CharInFragmentSet(c)) {
      ret += &URL_PARTS_CHARACTERS_ENCODE(c);
    } else {
      ret += c;
    }
  }
  return ret;
}

std::string EncodePathname(const std::string& orig) {
  std::string ret = "";
  unsigned char c;
  for (auto it = orig.begin(); it != orig.end(); it++) {
    c = *it;
    if (CharInPathSet(c)) {
      ret += &URL_PARTS_CHARACTERS_ENCODE(c);
    } else {
      ret += c;
    }
  }
  return ret;
}

std::string URLPartsTemporaryCharPtrToString(
    const URLPartsTemporaryCharPtr& ptr) {
  return std::string(ptr.start, ptr.length);
}

void ltrim(std::string& s) {  // NOLINT: runtime/references
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch) && !CharInC0ControlSet(ch);
          }));
}

void rtrim(std::string& s) {  // NOLINT: runtime/references
  s.erase(std::find_if(s.rbegin(),
                       s.rend(),
                       [](unsigned char ch) {
                         return !std::isspace(ch) && !CharInC0ControlSet(ch);
                       })
              .base(),
          s.end());
}

void trim(std::string& s) {  // NOLINT: runtime/references
  ltrim(s);
  rtrim(s);
}

bool IsSpecialProtocols(std::string protocol) {
  for (size_t i = 0; i < arraysize(kSpecialProtocols); i++) {
    if (protocol == kSpecialProtocols[i]) {
      return true;
    }
  }
  return false;
}

std::string ParseProtocol(const char* ptr, const char** out) {
  URLPartsTemporaryCharPtr result = {ptr, 0};

  // protocol should start with [a-zA-Z]
  if (!(*ptr >= 'A' && *ptr <= 'Z') && !(*ptr >= 'a' && *ptr <= 'z')) {
    return "";
  }

  result.length++;
  ptr++;

  while ((*ptr >= 'a' && *ptr <= 'z') || (*ptr >= 'A' && *ptr <= 'Z') ||
         (*ptr >= '0' && *ptr <= '9') || *ptr == '.' || *ptr == '-' ||
         *ptr == '+') {
    ptr++;
    result.length++;
  }

  if (*ptr == ':') {
    ptr++;
    result.length++;
  } else {
    *out = ptr;
    return "";
  }

  *out = ptr;
  return URLPartsTemporaryCharPtrToString(result);
}

enum URLPartsSlashCount { SINGLE = 1, DOUBLE, TRIPLE, INF, NONE, UPSC_MAX };

std::string ParseFileHostnameOrAuthority(const char* ptr,
                                         URLPartsSlashCount slash_count,
                                         const char** out) {
  switch (slash_count) {
      /*
      case URLPartsSlashCount::SINGLE:
        if (*ptr != '/' && *ptr != '\\') return "";
        ptr++;
        break;
        */
    case URLPartsSlashCount::NONE:
      break;

    case URLPartsSlashCount::DOUBLE:
      if (!IsDoubleSlash(ptr)) {
        *out = ptr;
        return "";
      }

      ptr += 2;
      break;

    case URLPartsSlashCount::INF:
      while (*ptr == '/' || *ptr == '\\') {
        ptr++;
      }
      break;

    default:
      char err[32];
      snprintf(err, sizeof(err), "invlid slash count %d", slash_count);
      UNREACHABLE(err);
  }

  URLPartsTemporaryCharPtr result = {ptr, 0};
  while (*ptr && *ptr != '/' && *ptr != '\\' && *ptr != '?' && *ptr != '#') {
    ptr++;
    result.length++;
  }

  *out = ptr;
  return URLPartsTemporaryCharPtrToString(result);
}

std::string ParseFileHostname(const char* ptr, const char** out) {
  return ParseFileHostnameOrAuthority(ptr, URLPartsSlashCount::DOUBLE, out);
}

std::string ParseNormalSchemeAuthority(const char* ptr,
                                       URLPartsSlashCount slashes,
                                       const char** out) {
  return ParseFileHostnameOrAuthority(ptr, slashes, out);
}

std::string ParseSpecialSchemeAuthority(const char* ptr, const char** out) {
  return ParseFileHostnameOrAuthority(ptr, URLPartsSlashCount::INF, out);
}

enum URLPartsAuthenticationStopSign { COLON = 0, AT, ZERO };

std::string ParseUsername(const char* start,
                          URLPartsAuthenticationStopSign* stop_sign,
                          const char** out) {
  const char* ptr = start;
  while (*ptr != ':' && *ptr != '@' &&
         *ptr != 0 /* != 0 is unreachable in normal process */) {
    ptr++;
  }

  if (*ptr == ':')
    *stop_sign = URLPartsAuthenticationStopSign::COLON;
  else if (*ptr == '@')
    *stop_sign = URLPartsAuthenticationStopSign::AT;
  else if (*ptr == 0)
    *stop_sign = URLPartsAuthenticationStopSign::ZERO;

  std::string result(start, ptr - start);

  start = ptr + (*ptr == 0 ? 0 : 1);

  *out = start;
  return EncodeUserInfo(result);
}

std::string ParsePassword(const char* start,
                          URLPartsAuthenticationStopSign* stop_sign,
                          const char** out) {
  const char* ptr = start;
  while (*ptr != '@' && *ptr != 0 /* != 0 is unreachable in normal process */) {
    ptr++;
  }

  *stop_sign = *ptr == 0 ? URLPartsAuthenticationStopSign::ZERO
                         : URLPartsAuthenticationStopSign::AT;

  std::string result(start, ptr - start);
  start = ptr + (*ptr == 0 ? 0 : 1);

  *out = start;
  return EncodeUserInfo(result);
}

std::string ParseHostname(const char* ptr, const char** out) {
  URLPartsTemporaryCharPtr result = {ptr, 0};
  bool inside_brackets = false;
  while (*ptr) {
    switch (*ptr) {
      case 0x5b:  // [
        inside_brackets = true;
        break;

      case 0x5d:  // ]
        inside_brackets = false;
        break;

      case 0x3a:  // :
        if (*ptr == 0x3a && !inside_brackets) {
          *out = ptr;
          return URLPartsTemporaryCharPtrToString(result);
        }
        break;

      default:
        break;
    }

    ptr++;
    result.length++;
  }

  *out = ptr;
  return URLPartsTemporaryCharPtrToString(result);
}

std::string EncodeHostname(const std::string& hostname,
                           bool is_special,
                           bool be_strict) {
  std::string result = "";

  // in ipv6 format
  if (hostname[0] == '[') {
    if (hostname[hostname.length() - 1] != ']') {
      return "";
    } else if (hostname.length() < 4) /* ^\[[0-9a-fA-F.:]{2,}\]$ */ {
      return "";
    }

    const char* inner = hostname.c_str() + 1;
    int inner_length = hostname.length() - 1;
    ResizableBuffer encoded(inner_length);
    int encoded_length = 0;
    ipvx::CompressIPv6Result encode_ret =
        ipvx::CompressIPv6(inner,
                           inner_length,
                           static_cast<char*>(encoded.buffer()),
                           &encoded_length);

    if (encode_ret == ipvx::CompressIPv6Result::kIPv6CompressFail) {
      return "";
    }

    result = std::string(static_cast<char*>(encoded.buffer()), encoded_length);
    return result;
  }

  // not special
  if (!is_special) {
    unsigned char c;
    // Check against forbidden host code points except for "%".
    for (auto it = hostname.begin(); it != hostname.end(); it++) {
      c = *it;
      if (CharIsForbiddenInHost(c) && c != '%') {
        return "";
      }

      // Percent encode C0 control set
      if (CharInC0ControlSet(c)) {
        result += &URL_PARTS_CHARACTERS_ENCODE(c);
      } else {
        result += c;
      }
    }
    return result;
  }

  // percent decode
  ResizableBuffer percent_decoded(hostname.length() + 1);
  int32_t decoded_len =
      uri_encode::Decode(hostname.c_str(),
                         hostname.length(),
                         static_cast<char*>(percent_decoded.buffer()));
  char* percent_decoded_ptr = static_cast<char*>(percent_decoded.buffer());
  if (strstr(percent_decoded_ptr, "%") != nullptr) {
    return "";
  }

  std::string asciied = "";
  int32_t len =
      domainToASCII(&asciied, percent_decoded_ptr, decoded_len, be_strict);
  if (len < 0) {
    return "";
  }

  // Check against forbidden host code points
  const unsigned char* asciied_ptr =
      reinterpret_cast<const unsigned char*>(asciied.c_str());
  while (*asciied_ptr != '\0') {
    if (CharIsForbiddenInHost(*percent_decoded_ptr)) {
      return "";
    }
    asciied_ptr++;
  }

  // IPv4 parsing
  if (is_special) {
    char ip[MAX_FORMATTED_IPV4_LEN + 1];
    int ip_len;
    ipvx::FormatIPv4Result transform_ipv4_result =
        ipvx::FormatIPv4(asciied.c_str(), asciied.length(), ip, &ip_len);
    if (transform_ipv4_result == ipvx::FormatIPv4Result::kFormatIPv4Fail) {
      return "";
    } else if (transform_ipv4_result == ipvx::FormatIPv4Result::kFormatIPv4Ok) {
      return ip;
    }

    // else: not an ipv4 address -> ignore
  }

  return asciied;
}

std::string GetPort(const char* rest) {
  if (*rest != ':') {
    return "";
  } else {
    return std::string(rest + 1);
  }
}

std::string GetPathname(const char* ptr, const char** out) {
  std::string result = "";
  while (*ptr && *ptr != '?' && *ptr != '#') {
    result += (*ptr == '\\') ? '/' : *ptr;
    ptr++;
  }

  *out = ptr;
  return result;
}

std::string GetQuery(const char* ptr, const char** out) {
  URLPartsTemporaryCharPtr result = {ptr, 0};
  while (*ptr && *ptr != '#') {
    ptr++;
    result.length++;
  }

  *out = ptr;
  return URLPartsTemporaryCharPtrToString(result);
}

bool IsWindowsDriveLetters(const char* path) {
  return ((*path >= 'a' && *path <= 'z') || (*path >= 'A' && *path <= 'Z')) &&
         *(path + 1) == ':';
}

std::string NormalizePathname(const std::string& path, bool is_file_path) {
  if (!path.length()) return path;

  // TODO(kaidi.zkd): is this length enough?
  ResizableBuffer buff(path.length() + 1);
  bool last_slash = path[path.length() - 1] == '/';
  bool is_absolute = path[0] == '/';
  std::string result = "";
  std::string driver = "";
  const char* ptr = path.c_str();
  CWalkPathStyleScope path_style_scope;

  while (*ptr == '/') ptr++;

  bool is_windows = false;
  if (is_file_path && IsWindowsDriveLetters(ptr)) {
    path_style_scope.SetStyle(CWK_STYLE_WINDOWS);
    is_windows = true;
  } else {
    path_style_scope.SetStyle(CWK_STYLE_UNIX);
    if (is_absolute) {
      ptr--;
    }
  }
  cwk_path_normalize(ptr,
                     static_cast<char*>(buff.buffer()),
                     (path.length() + 1) * sizeof(char));

  result = static_cast<char*>(buff.buffer());
  if (is_windows && is_absolute) {
    result = "/" + result;
  }

  if ((!result.length() || (result[result.length() - 1] != '/')) &&
      last_slash) {
    result += "/";
  }

  return EncodePathname(result);
}

std::string ResolvePathnameFromBase(const std::string& path,
                                    const std::string& base_path,
                                    bool is_file_path) {
  static std::string const root = "/";
  if (!base_path.length() && !path.length()) return "";

  std::string result;
  std::string base_prefix;
  std::string suffix;
  const char* base_path_ptr =
      !base_path.length() ? root.c_str() : base_path.c_str();
  const char* path_ptr = path.c_str();
  bool last_slash = path[path.length() - 1] == '/';

  bool is_path_absolute = *path_ptr == '/';
  bool is_base_absolute = *base_path_ptr == '/';
  while (*base_path_ptr == '/') base_path_ptr++;
  while (*path_ptr == '/') path_ptr++;

  // TODO(kaidi.zkd): is this length enough?
  ResizableBuffer buff(path.length() + base_path.length() + 1);
  CWalkPathStyleScope path_style_scope(CWK_STYLE_UNIX);

  if (is_file_path && IsWindowsDriveLetters(path_ptr)) {
    if (is_path_absolute) path_ptr--;
    const char* path_ptr = path.c_str();
    return NormalizePathname(GetPathname(path_ptr, &path_ptr), is_file_path);
  } else if (is_path_absolute) {
    if (is_file_path && IsWindowsDriveLetters(base_path_ptr)) {
      path_style_scope.SetStyle(CWK_STYLE_WINDOWS);
      base_prefix = *base_path_ptr + *(base_path_ptr + 1) + '/';
    } else {
      base_prefix = root;
    }

    const char* temp = path.c_str();
    suffix = GetPathname(temp, &temp);
    temp = base_prefix.c_str();
    base_prefix = GetPathname(temp, &temp);

    cwk_path_get_absolute(base_prefix.c_str(),
                          suffix.c_str(),
                          static_cast<char*>(buff.buffer()),
                          buff.byte_length());
    result = std::string(static_cast<char*>(buff.buffer()));
    if ((!result.length() || (result[result.length() - 1] != '/')) &&
        last_slash) {
      result += '/';
    }

    if (is_base_absolute && result[0] != '/') {
      result = '/' + result;
    }

    return EncodePathname(result);
  } else if (path != "") {
    base_prefix = GetPathname(base_path_ptr, &base_path_ptr);
    const char* path_ptr = path.c_str();
    suffix = GetPathname(path_ptr, &path_ptr);
    if (is_file_path && IsWindowsDriveLetters(base_path_ptr)) {
      path_style_scope.SetStyle(CWK_STYLE_WINDOWS);
    }

    // base tail slash
    ResizableBuffer base_buff(0);
    if (base_prefix[base_prefix.length() - 1] != '/') {
      base_buff.Realloc(base_prefix.length() + 1);
      cwk_path_join(base_prefix.c_str(),
                    "..",
                    static_cast<char*>(base_buff.buffer()),
                    base_buff.byte_length());
      base_path_ptr = static_cast<char*>(base_buff.buffer());
    } else {
      base_path_ptr = base_prefix.c_str();
    }

    cwk_path_get_absolute(base_path_ptr,
                          suffix.c_str(),
                          static_cast<char*>(buff.buffer()),
                          buff.byte_length());
    result = std::string(static_cast<char*>(buff.buffer()));
    if ((!result.length() || (result[result.length() - 1] != '/')) &&
        last_slash) {
      result += '/';
    }

    if (is_base_absolute && result[0] != '/') {
      result = '/' + result;
    }

    return EncodePathname(result);
  } else {
    base_path_ptr = base_path.c_str();
    base_prefix = GetPathname(base_path_ptr, &base_path_ptr);
    return NormalizePathname(base_prefix, is_file_path);
  }
}

bool IsValidPort(const std::string& port) {
  if (port == "") {
    return true;
  } else {
    if (port.length() > 5) {
      return false;
    }

    unsigned int port_number;
    ipvx::TransformIPv4SegmentResult transform_result =
        ipvx::TransformIPv4SegmentFromDec(
            const_cast<char*>(port.c_str()), 65535, &port_number);
    return transform_result == ipvx::TransformIPv4SegmentResult::kTransformOk;
  }
}
}  // namespace

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

Url::Url(const std::string& url) {
  Parse(url);
}

Url::Url(const std::string& url, const std::string& base) {
  Url base_url(base);
  Parse(url, &base_url);
}

void Url::operator=(const Url& other) {
  is_valid_ = other.is_valid_;
  protocol_ = other.protocol_;
  slashes_ = other.slashes_;
  username_ = other.username_;
  password_ = other.password_;
  hostname_ = other.hostname_;
  port_ = other.port_;
  pathname_ = other.pathname_;
  search_ = other.search_;
  hash_ = other.hash_;
}

// TODO(chengzhong.wcz): strict conformance to the state of
// https://url.spec.whatwg.org/#concept-basic-url-parser
void Url::Parse(const std::string& url, const Url* base) {
  if (base && !base->is_valid_) {
    is_valid_ = false;
    return;
  }

  bool used_non_base = false;

  std::string trimed_url = url;
  trim(trimed_url);

  const char* start = trimed_url.c_str();
  const char* ptr = start;

  // step 1. parse protocol
  std::string protocol = ParseProtocol(ptr, &ptr);
  bool is_special = IsSpecialProtocols(protocol);
  std::transform(
      protocol.begin(), protocol.end(), protocol.begin(), [](unsigned char c) {
        return std::tolower(c);
      });
  protocol_ = protocol;
  if (!protocol.length()) {
    if (!base) {
      is_valid_ = false;
      return;
    }

    protocol_ = base->protocol_;
    ptr = start;
  } else if ((!base || base->protocol_ != protocol) || !is_special) {
    used_non_base = true;
  }

  std::string hostname;
  if (protocol_ == "file:") {
    // step 2.1. parse to hostname (file://)
    slashes_ = "//";
    username_ = "";
    password_ = "";
    port_ = "";

    hostname = ParseFileHostname(ptr, &ptr);
    if (used_non_base || hostname.length()) {
      hostname_ = hostname;
      used_non_base = true;
    } else {
      hostname_ = base->hostname_;
    }
  } else {
    // step 2.2. parse to hostname (others)
    static const char at[] = "@";
    bool is_next_double_slash = IsDoubleSlash(ptr);
    if (used_non_base && !IsSlash(ptr)) {
      // Do nothing to skip `hostname` parsing
    } else if (used_non_base || is_next_double_slash) {
      slashes_ = (is_special || is_next_double_slash) ? "//" : "";
      std::string rest_authority =
          is_special ? ParseSpecialSchemeAuthority(ptr, &ptr)
                     : ParseNormalSchemeAuthority(
                           ptr,
                           is_next_double_slash ? URLPartsSlashCount::DOUBLE
                                                : URLPartsSlashCount::NONE,
                           &ptr);
      const char* rest_authority_ptr = rest_authority.c_str();

      // `username[:password]@` found
      if (strstr(rest_authority_ptr, at) != nullptr) {
        URLPartsAuthenticationStopSign stop_sign =
            URLPartsAuthenticationStopSign::AT;
        std::string username =
            ParseUsername(rest_authority_ptr, &stop_sign, &rest_authority_ptr);
        CHECK_NE(stop_sign, URLPartsAuthenticationStopSign::ZERO);
        std::string password =
            stop_sign == URLPartsAuthenticationStopSign::AT
                ? ""
                : ParsePassword(
                      rest_authority_ptr, &stop_sign, &rest_authority_ptr);
        CHECK_EQ(stop_sign, URLPartsAuthenticationStopSign::AT);

        username_ = username;
        password_ = password;
      } else {
        username_ = "";
        password_ = "";
      }

      hostname = ParseHostname(rest_authority_ptr, &rest_authority_ptr);
      if (hostname == "" && is_special) {
        is_valid_ = false;
        return;
      }

      std::string port = GetPort(rest_authority_ptr);
      if (!IsValidPort(port)) {
        is_valid_ = false;
        return;
      } else {
        port_ = port;
      }

      used_non_base = true;
    } else {
      CHECK_NE(base, nullptr);
      slashes_ = base->slashes_;
      username_ = base->username_;
      password_ = base->password_;
      port_ = base->port_;

      hostname = base->hostname_;
    }

    if (hostname != "") {
      hostname = EncodeHostname(hostname, is_special, false);

      if (hostname == "") {
        is_valid_ = false;
        return;
      }
      hostname_ = hostname;
    } else {
      hostname_ = "";
    }
  }

  // `ptr` is not in `path` segment
  std::string pathname = GetPathname(ptr, &ptr);
  if (used_non_base) {
    pathname = NormalizePathname(pathname, protocol_ == "file:");
  } else {
    if (pathname != "") {
      used_non_base = true;
    }

    pathname = ResolvePathnameFromBase(
        pathname, base->pathname_, protocol_ == "file:");
  }

  pathname_ = pathname;

  // drop the hostname if a drive letter is parsed
  if (protocol_ == "file:" && pathname.length() >= 4 &&
      (pathname[0] == '/' &&
       ((pathname[1] >= 'a' && pathname[1] <= 'z') ||
        (pathname[1] >= 'A' && pathname[1] <= 'Z')) &&
       pathname[2] == ':' &&
       (pathname[3] == '/' || pathname[3] == '|' || pathname[3] == '$'))) {
    hostname_ = "";
  }

  if (used_non_base || *ptr == '?') {
    search_ = EncodeQuery(GetQuery(ptr, &ptr), is_special);
    used_non_base = true;
  } else {
    search_ = base->search_;
  }

  hash_ = EncodeHash(ptr);
  is_valid_ = true;
}

bool Url::is_valid() const {
  return is_valid_;
}

std::string Url::Origin() const {
  std::string value;
  std::string host = Host();
  if (host.length()) {
    value = protocol_ + "//" + host;
  } else {
    value = "null";
  }
  return value;
}

std::string Url::Protocol() const {
  return protocol_;
}

std::string Url::Username() const {
  return username_;
}

std::string Url::Password() const {
  return password_;
}

std::string Url::Host() const {
  std::string value = hostname_;
  std::string port = Port();
  if (port.length()) {
    value += ':' + port;
  }

  return value;
}

std::string Url::Hostname() const {
  return hostname_;
}

std::string Url::Port() const {
  std::string value = port_;
  auto it = kSpecialProtocolPorts.find(protocol_);
  if (it != kSpecialProtocolPorts.end() && it->second == value) {
    value = "";
  }

  return value;
}

std::string Url::Pathname() const {
  if (IsSpecialProtocols(protocol_)) {
    return !pathname_.length()
               ? kSlash
               : ((*pathname_.c_str() != '/' && *pathname_.c_str() != '\\'
                       ? kSlash
                       : "") +
                  pathname_);
  }

  return pathname_;
}

std::string Url::Search() const {
  if (search_.length() == 1 && search_ == "?") return "";
  return search_;
}

std::string Url::Hash() const {
  return hash_;
}

std::string Url::Href() const {
  std::string authentication = username_.length() || password_.length()
                                   ? username_ + ':' + password_ + '@'
                                   : "";
  std::string host = Host();
  std::string slashes = host.length() ? "//" : slashes_;
  std::string pathname = Pathname();
  if (pathname.length() && pathname[0] != '/' && host.length()) {
    pathname = '/' + pathname;
  }

  std::string value =
      protocol_ + slashes + authentication + host + pathname + Search() + hash_;
  return value;
}

void Url::SetProtocol(const std::string& val) {
  protocol_ = UpdateProtocol(val);
}

void Url::SetUsername(const std::string& val) {
  username_ = UpdateUserInfo(val);
}

void Url::SetPassword(const std::string& val) {
  password_ = UpdateUserInfo(val);
}

void Url::SetHost(const std::string& val) {
  std::pair<std::string, std::string> result = UpdateHost(protocol_, val);
  if (result.first.length() == 0) {
    return;
  }
  hostname_ = result.first;
  if (result.second.length() == 0) {
    return;
  }
  port_ = result.second;
}

void Url::SetHostname(const std::string& val) {
  std::string result = UpdateHostname(protocol_, val);
  if (result.length() == 0) {
    return;
  }
  hostname_ = result;
}

void Url::SetPort(const std::string& val) {
  std::string result = UpdatePort(protocol_, val);
  if (result.length() == 0) {
    return;
  }
  port_ = result;
}

void Url::SetPathname(const std::string& val) {
  pathname_ = UpdatePathname(protocol_, val);
}

void Url::SetSearch(const std::string& val) {
  search_ = UpdateSearch(protocol_, val);
}

void Url::SetHash(const std::string& val) {
  hash_ = UpdateHash(val);
}

bool Url::MaybeSetHref(const std::string& val) {
  Url url(val);
  if (!url.is_valid()) {
    return false;
  }
  *this = url;
  return true;
}

// static
std::string Url::UpdateProtocol(const std::string& val) {
  std::string protocol = "";
  for (auto it = val.begin(); it != val.end(); it++) {
    if (*it == 0x1f || *it == 0) {
      return "";
    } else if (CharInC0ControlSet(*it)) {
      continue;
    } else {
      protocol += *it;
    }
  }

  while (protocol.length() && protocol[protocol.length() - 1] == ':') {
    protocol = protocol.substr(0, protocol.length() - 1);
  }
  return protocol + ":";
}

// static
std::string Url::UpdateUserInfo(const std::string& val) {
  return EncodeUserInfo(val);
}

// static
std::pair<std::string, std::string> Url::UpdateHost(const std::string& protocol,
                                                    const std::string& host) {
  std::string hostname;
  std::string port;
  size_t colon_pos = host.find(':');
  if (colon_pos == std::string::npos) {
    hostname = host;
  } else {
    hostname = host.substr(0, colon_pos);
    port = host.substr(colon_pos + 1);
  }

  hostname = UpdateHostname(protocol, hostname);

  if (hostname.length() > 0 && colon_pos != std::string::npos) {
    return std::make_pair(hostname, port);
  }
  return std::make_pair(hostname, "");
}

// static
std::string Url::UpdateHostname(const std::string& protocol,
                                const std::string& val) {
  std::string hostname = "";
  for (auto it = val.begin(); it != val.end(); it++) {
    if (*it == 0) return hostname;

    if (!std::isspace(*it)) {
      hostname += *it;
    }
  }

  bool is_special = IsSpecialProtocols(protocol);
  if (!hostname.length()) {
    return hostname;
  }

  return EncodeHostname(hostname, is_special, true);
}

// static
std::string Url::UpdatePort(const std::string& protocol,
                            const std::string& val) {
  std::string port;
  for (auto it = val.begin(); it != val.end(); it++) {
    if (*it >= '0' && *it <= '9') {
      port += *it;
    } else if (*it == 0x1f || *it == 0) {
      break;
    } else {
      continue;
    }
  }

  if (!port.length()) return "";
  if (!IsValidPort(port)) return "";

  auto it = kSpecialProtocolPorts.find(protocol);
  if (it != kSpecialProtocolPorts.end() && it->second == port) {
    port = "";
  }

  return port;
}

// static
std::string Url::UpdatePathname(const std::string& protocol,
                                const std::string& val) {
  std::string pathname = "";
  for (auto it = val.begin(); it != val.end(); it++) {
    if (!CharInC0ControlSet(*it) || *it == 0x1f || *it == 0) pathname += *it;
  }
  if (pathname.length() && pathname[0] != '/' && pathname[0] != '\\') {
    pathname = '/' + pathname;
  }

  pathname = EncodePathname(pathname);

  return NormalizePathname(pathname, protocol == "file:");
}

// static
std::string Url::UpdateSearch(const std::string& protocol,
                              const std::string& val) {
  std::string search = "";
  for (auto it = val.begin(); it != val.end(); it++) {
    if (!CharInC0ControlSet(*it) || *it == 0x1f || *it == 0) search += *it;
  }
  if (search.length() && search[0] != '?') search = '?' + search;
  bool is_special = IsSpecialProtocols(protocol);

  return EncodeQuery(search, is_special);
}

// static
std::string Url::UpdateHash(const std::string& val) {
  std::string hash = "";
  for (auto it = val.begin(); it != val.end(); it++) {
    if (!CharInC0ControlSet(*it) || *it == 0x1f || *it == 0) hash += *it;
  }

  hash = EncodeHash(hash);
  if (hash[0] != '#') hash = '#' + hash;
  return hash;
}

}  // namespace aworker
