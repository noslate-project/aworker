#ifndef SRC_UTILS_CONVENIENT_ARRAY_BUFFER_STORE_H_
#define SRC_UTILS_CONVENIENT_ARRAY_BUFFER_STORE_H_

#include "v8.h"

namespace aworker {

class ConvenientArrayBufferStore {
 private:
  using Address = char*;

 public:
  inline ConvenientArrayBufferStore(v8::Local<v8::ArrayBuffer> array_buffer,
                                    size_t offset = 0)
      : _backing_store(array_buffer->GetBackingStore()),
        _data(ExtractBuffer(_backing_store, offset)) {
    _byte_length = array_buffer->ByteLength() - offset;
  }

  inline ConvenientArrayBufferStore(v8::Local<v8::ArrayBuffer> array_buffer,
                                    size_t offset,
                                    size_t length)
      : _backing_store(array_buffer->GetBackingStore()),
        _data(ExtractBuffer(_backing_store, offset)) {
    _byte_length = length;
  }

  inline ~ConvenientArrayBufferStore() {}

  inline void* data() { return _data; }
  inline size_t byte_length() { return _byte_length; }

 private:
  inline static Address ExtractBuffer(
      const std::shared_ptr<v8::BackingStore>& backing_store, size_t offset) {
    Address data = static_cast<Address>(backing_store->Data());
    return data + offset;
  }

 protected:
  std::shared_ptr<v8::BackingStore> _backing_store;
  Address _data;
  size_t _byte_length;
};

}  // namespace aworker

#endif  // SRC_UTILS_CONVENIENT_ARRAY_BUFFER_STORE_H_
