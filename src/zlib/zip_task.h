#ifndef SRC_ZLIB_ZIP_TASK_H_
#define SRC_ZLIB_ZIP_TASK_H_

#include "zlib_task.h"

namespace aworker {
namespace zlib {

class ZipTask : public ZlibTask {
 public:
  ZipTask(Immortal* immortal,
          ZlibWrapper* wrapper,
          v8::Local<v8::Object> life_object,
          v8::Local<v8::ArrayBuffer> array_buffer,
          size_t array_buffer_offset,
          size_t array_buffer_length,
          int32_t flush_mode)
      : ZlibTask(immortal,
                 wrapper,
                 life_object,
                 array_buffer,
                 array_buffer_offset,
                 array_buffer_length,
                 flush_mode) {}

  void OnWorkTick() override;
};

}  // namespace zlib
}  // namespace aworker

#endif  // SRC_ZLIB_ZIP_TASK_H_
