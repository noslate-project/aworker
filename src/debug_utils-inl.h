#pragma once
#include <uv.h>
#include <type_traits>
#include "aworker_perf.h"
#include "debug_utils.h"
#include "util.h"

namespace aworker {

template <typename... Args>
inline void FORCE_INLINE Debug(EnabledDebugList* list,
                               DebugCategory cat,
                               const char* format,
                               Args&&... args) {
  if (!UNLIKELY(list->enabled(cat))) return;
  FPrintF(stderr, format, std::forward<Args>(args)...);
}

inline void FORCE_INLINE Debug(EnabledDebugList* list,
                               DebugCategory cat,
                               const char* message) {
  if (!UNLIKELY(list->enabled(cat))) return;
  FPrintF(stderr, "%s", message);
}

struct ToStringHelper {
  template <typename T>
  static std::string Convert(const T& value,
                             std::string (T::*to_string)()
                                 const = &T::ToString) {
    return (value.*to_string)();
  }
  template <
      typename T,
      typename test_for_number =
          typename std::enable_if<std::is_arithmetic<T>::value, bool>::type,
      typename dummy = bool>
  static std::string Convert(const T& value) {
    return std::to_string(value);
  }
  static std::string Convert(const char* value) {
    return value != nullptr ? value : "(null)";
  }
  static std::string Convert(const std::string& value) { return value; }
  static std::string Convert(bool value) { return value ? "true" : "false"; }
  template <unsigned BASE_BITS,
            typename T,
            typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
  static std::string BaseConvert(const T& value) {
    auto v = static_cast<uint64_t>(value);
    char ret[3 * sizeof(T)];
    char* ptr = ret + 3 * sizeof(T) - 1;
    *ptr = '\0';
    const char* digits = "0123456789abcdef";
    do {
      unsigned digit = v & ((1 << BASE_BITS) - 1);
      *--ptr = (BASE_BITS < 4 ? static_cast<char>('0' + digit) : digits[digit]);
    } while ((v >>= BASE_BITS) != 0);
    return ptr;
  }
  template <unsigned BASE_BITS,
            typename T,
            typename std::enable_if<!std::is_integral<T>::value, int>::type = 0>
  static std::string BaseConvert(T value) {
    return Convert(std::forward<T>(value));
  }
};

template <typename T>
std::string ToString(const T& value) {
  return ToStringHelper::Convert(value);
}

template <unsigned BASE_BITS, typename T>
std::string ToBaseString(const T& value) {
  return ToStringHelper::BaseConvert<BASE_BITS>(value);
}

inline std::string SPrintFImpl(const char* format) {
  const char* p = strchr(format, '%');
  if (LIKELY(p == nullptr)) return format;
  CHECK_EQ(p[1], '%');  // Only '%%' allowed when there are no arguments.

  return std::string(format, p + 1) + SPrintFImpl(p + 2);
}

template <typename Arg, typename... Args>
std::string COLD_NOINLINE SPrintFImpl(  // NOLINT(runtime/string)
    const char* format,
    Arg&& arg,
    Args&&... args) {
  const char* p = strchr(format, '%');
  CHECK_NOT_NULL(p);  // If you hit this, you passed in too many arguments.
  std::string ret(format, p);
  // Ignore long / size_t modifiers
  while (strchr("lz", *++p) != nullptr) {
  }
  switch (*p) {
    case '%': {
      return ret + '%' +
             SPrintFImpl(
                 p + 1, std::forward<Arg>(arg), std::forward<Args>(args)...);
    }
    default: {
      return ret + '%' +
             SPrintFImpl(
                 p, std::forward<Arg>(arg), std::forward<Args>(args)...);
    }
    case 'd':
    case 'i':
    case 'u':
    case 's':
      ret += ToString(arg);
      break;
    case 'o':
      ret += ToBaseString<3>(arg);
      break;
    case 'x':
      ret += ToBaseString<4>(arg);
      break;
    case 'X':
      ret += aworker::ToUpper(ToBaseString<4>(arg));
      break;
    case 'p': {
      CHECK(std::is_pointer<typename std::remove_reference<Arg>::type>::value);
      char out[20];
      int n = snprintf(
          out, sizeof(out), "%p", *reinterpret_cast<const void* const*>(&arg));
      CHECK_GE(n, 0);
      ret += out;
      break;
    }
  }
  return ret + SPrintFImpl(p + 1, std::forward<Args>(args)...);
}

template <typename... Args>
std::string COLD_NOINLINE SPrintF(  // NOLINT(runtime/string)
    const char* format,
    Args&&... args) {
  return SPrintFImpl(format, std::forward<Args>(args)...);
}

template <typename... Args>
void COLD_NOINLINE FPrintF(FILE* file, const char* format, Args&&... args) {
  FWrite(file, SPrintF(format, std::forward<Args>(args)...));
}

namespace per_process {

template <typename... Args>
inline void FORCE_INLINE Debug(DebugCategory cat,
                               const char* format,
                               Args&&... args) {
  Debug(&enabled_debug_list, cat, format, std::forward<Args>(args)...);
}

inline void FORCE_INLINE Debug(DebugCategory cat, const char* message) {
  Debug(&enabled_debug_list, cat, message);
}

inline void FORCE_INLINE DebugPrintCurrentTime(const char* name) {
  if (!UNLIKELY(enabled_debug_list.enabled(DebugCategory::PERFORMANCE))) return;

  uint64_t hrtime = uv_hrtime() - getTimeOrigin();
  fprintf(stderr,
          "[print time] %s: %f\n",
          name,
          /** integer part unit: millisecond */ hrtime / 1e6);
}

}  // namespace per_process
}  // namespace aworker
