#ifndef SRC_ZLIB_UNZIP_H_
#define SRC_ZLIB_UNZIP_H_

#include "zlib_wrapper.h"

namespace aworker {
namespace zlib {

enum class UnzipType { GUNZIP = 0, INFLATE, UNZIP, UNZIP_TYPE_MAX };
enum class UnzipFirstTwoBytesParsedStatus {
  kNonParsed = 0,
  kFirstParsed = 1,
  kSecondParsed = 2
};

class UnzipWrapper : public ZlibWrapper {
  DEFINE_WRAPPERTYPEINFO();
  SIZE_IN_BYTES(UnzipWrapper)
  SET_NO_MEMORY_INFO()

 public:
  static int TypeEnumMax() {
    return static_cast<int>(UnzipType::UNZIP_TYPE_MAX);
  }

 public:
  UnzipWrapper(Immortal* immortal,
               v8::Local<v8::Object> handle,
               UnzipType type,
               bool is_sync,
               int window_bits,
               v8::Local<v8::Value> _);
  ~UnzipWrapper() override;

  bool Init() override;
  v8::MaybeLocal<v8::Value> Push(v8::Local<v8::Object> this_object,
                                 v8::Local<v8::ArrayBuffer> array_buffer,
                                 size_t array_buffer_offset,
                                 size_t array_buffer_length,
                                 int32_t flush_mode,
                                 v8::Local<v8::Function> callback) override;
  void DoZlibEnd() override;

 private:
  // The leading two bytes is used for judging Magic Number when doing
  // `UnzipType::UNZIP`. So `_first_two_bytes_parsed_status` is used for
  // recording which byte is scaned and parsed.
  UnzipFirstTwoBytesParsedStatus _first_two_bytes_parsed_status;
  std::unique_ptr<ZlibTask> _head_task;
};

}  // namespace zlib
}  // namespace aworker

#endif  // SRC_ZLIB_UNZIP_H_
