#pragma once
#include "util.h"

namespace aworker {

/**
 * A simple Result type, representing an object which may or may not have a
 * value, used for returning and propagating errors.
 *
 * If an API method returns a Result<>, the API method can potentially fail
 * either because an exception is occurred. In that case, a "Result::Error"
 * value is returned.
 *
 * Caveat: The value/error would go out of scope if one of
 * ToValueChecked/ToValue/ToErrorChecked/ToError is been invoked.
 *
 * TODO(chengzhong.wcz): apply type constraints
 * TODO(chengzhong.wcz): inplace constructions
 */
template <typename value_type, typename error_type>
class Result {
 public:
  static inline Result<value_type, error_type> Ok(const value_type& value) {
    return Result(true, value, error_type());
  }
  static inline Result<value_type, error_type> Ok(value_type&& value) {
    return Result(true, std::move(value), error_type());
  }
  static inline Result<value_type, error_type> Error(const error_type& error) {
    return Result(false, value_type(), error);
  }
  static inline Result<value_type, error_type> Error(error_type&& error) {
    return Result(false, value_type(), std::move(error));
  }

  inline bool IsOk() const { return _has_value; }
  inline bool IsError() const { return !_has_value; }

  inline value_type ToValueChecked() {
    CHECK(IsOk());
    return std::move(_value);
  }
  inline bool ToValue(value_type* out) {
    if (LIKELY(IsOk())) *out = std::move(_value);
    return IsOk();
  }

  inline error_type ToErrorChecked() {
    CHECK(IsError());
    return std::move(_error);
  }
  inline bool ToError(error_type* out) {
    if (LIKELY(IsError())) *out = std::move(_error);
    return IsError();
  }

 private:
  explicit Result(bool has_value, value_type value, error_type error)
      : _has_value(has_value),
        _value(std::move(value)),
        _error(std::move(error)) {}

  bool _has_value;
  // TODO(chengzhong.wcz): mutual exclusive storage
  value_type _value;
  error_type _error;
};

}  // namespace aworker
