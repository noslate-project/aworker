#ifndef SRC_ZLIB_ZIP_H_
#define SRC_ZLIB_ZIP_H_

#include "zlib_wrapper.h"

namespace aworker {
namespace zlib {

enum class ZipType { GZIP = 0, DEFLATE, ZIP_TYPE_MAX };

class ZipWrapper : public ZlibWrapper {
  DEFINE_WRAPPERTYPEINFO();
  SIZE_IN_BYTES(ZipWrapper)
  SET_NO_MEMORY_INFO()

 public:
  static int TypeEnumMax() { return static_cast<int>(ZipType::ZIP_TYPE_MAX); }

 public:
  ZipWrapper(Immortal* immortal,
             v8::Local<v8::Object> handle,
             ZipType type,
             bool is_sync,
             int window_bits,
             v8::Local<v8::Value> options);
  ~ZipWrapper() override;

  bool Init() override;
  v8::MaybeLocal<v8::Value> Push(v8::Local<v8::Object> this_object,
                                 v8::Local<v8::ArrayBuffer> array_buffer,
                                 size_t array_buffer_offset,
                                 size_t array_buffer_length,
                                 int32_t flush_mode,
                                 v8::Local<v8::Function> callback) override;
  void DoZlibEnd() override;

 private:
  int _level;
  int _mem_level;
  int _strategy;
};

}  // namespace zlib
}  // namespace aworker

#endif  // SRC_ZLIB_ZIP_H_
