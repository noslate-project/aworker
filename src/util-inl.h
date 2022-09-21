#pragma once
#include "util.h"

namespace aworker {

template <typename T>
inline T MultiplyWithOverflowCheck(T a, T b) {
  auto ret = a * b;
  if (a != 0) CHECK_EQ(b, ret / a);

  return ret;
}

SlicedArguments::SlicedArguments(
    const v8::FunctionCallbackInfo<v8::Value>& args, size_t start)
    : std::vector<v8::Local<v8::Value>>() {
  const size_t length = static_cast<size_t>(args.Length());
  if (start >= length) return;
  const size_t size = length - start;

  resize(size);
  for (size_t i = 0; i < size; ++i) {
    (*this)[i] = args[i + start];
  }
}

char ToLower(char c) {
  return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

std::string ToLower(const std::string& in) {
  std::string out(in.size(), 0);
  for (size_t i = 0; i < in.size(); ++i) out[i] = ToLower(in[i]);
  return out;
}

char ToUpper(char c) {
  return c >= 'a' && c <= 'z' ? (c - 'a') + 'A' : c;
}

std::string ToUpper(const std::string& in) {
  std::string out(in.size(), 0);
  for (size_t i = 0; i < in.size(); ++i) out[i] = ToUpper(in[i]);
  return out;
}
}  // namespace aworker
