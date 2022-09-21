#pragma once

#include <cstdlib>
#include <cstring>

#include "util.h"

namespace aworker {

struct ReleasedResizableBuffer {
  void* buff;
  size_t byte_length;
};

class ResizableBuffer {
 private:
  using Address = char*;

 public:
  inline ResizableBuffer() : _buf(nullptr), _byte_length(0) {}

  explicit inline ResizableBuffer(size_t byte_length) : ResizableBuffer() {
    this->Realloc(byte_length);
  }

  AWORKER_DISALLOW_ASSIGN_COPY(ResizableBuffer);

  inline ResizableBuffer(ResizableBuffer&& other)
      : _buf(other._buf), _byte_length(other._byte_length) {
    other._buf = nullptr;
    other._byte_length = 0;
  }

  inline ResizableBuffer& operator=(ResizableBuffer&& other) {
    std::free(_buf);
    _buf = other._buf;
    _byte_length = other._byte_length;
    other._buf = nullptr;
    other._byte_length = 0;
    return *this;
  }

  inline ~ResizableBuffer() { std::free(_buf); }

  inline ReleasedResizableBuffer Release() {
    Address buf = _buf;
    auto byte_length = _byte_length;

    this->_buf = nullptr;
    this->_byte_length = 0;

    return {buf, byte_length};
  }

  inline void Realloc(size_t byte_length) {
    if (byte_length == _byte_length) return;

    _byte_length = byte_length;
    if (byte_length == 0) {
      std::free(_buf);
      _buf = nullptr;
      return;
    }

    _buf = static_cast<Address>(std::realloc(_buf, byte_length));
    CHECK_NOT_NULL(_buf);
  }

  inline void* operator*() { return _buf; }
  inline void* buffer() const { return _buf; }
  inline size_t byte_length() const { return _byte_length; }

  inline void Concat(const ResizableBuffer& resizable) {
    this->Concat(resizable, resizable.byte_length());
  }

  inline void Concat(const ResizableBuffer& resizable, size_t byte_length) {
    if (!byte_length) return;

    void* ptr = nullptr;

    if (_buf == nullptr) {
      this->Realloc(byte_length);
      ptr = _buf;
    } else {
      size_t self_byte_length = _byte_length;
      this->Realloc(self_byte_length + byte_length);
      ptr = _buf + self_byte_length;
    }

    memcpy(ptr, resizable.buffer(), byte_length);
  }

  inline void Prepend(const ResizableBuffer& resizable) {
    this->Prepend(resizable, resizable.byte_length());
  }

  inline void Prepend(const ResizableBuffer& resizable, size_t byte_length) {
    if (!byte_length) return;

    Address ptr = nullptr;

    if (_buf == nullptr) {
      this->Realloc(byte_length);
      ptr = _buf;
    } else {
      size_t self_byte_length = _byte_length;
      this->Realloc(self_byte_length + byte_length);
      // The src and target memory buffer may overlap, do not use memcpy as
      // overlapping is undefined behavior in memcpy.
      std::memmove(_buf + byte_length, _buf, self_byte_length);
      ptr = _buf;
    }

    std::memcpy(ptr, resizable.buffer(), byte_length);
  }

 private:
  Address _buf;
  size_t _byte_length;
};

}  // namespace aworker
