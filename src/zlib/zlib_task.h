#ifndef SRC_ZLIB_ZLIB_TASK_H_
#define SRC_ZLIB_ZLIB_TASK_H_

extern "C" {
#include <zlib.h>
}

#include "immortal.h"
#include "macro_task_queue.h"
#include "utils/convenient_array_buffer_store.h"
#include "utils/resizable_buffer.h"

namespace aworker {
namespace zlib {

using ZlibStreamPtr = z_stream*;

class ZlibWrapper;
class ZlibTask : public MacroTask {
  friend class ZlibWrapper;

 public:
  ZlibTask(Immortal* immortal,
           ZlibWrapper* wrapper,
           v8::Local<v8::Object> life_object,
           v8::Local<v8::ArrayBuffer> array_buffer,
           size_t array_buffer_offset,
           size_t array_buffer_length,
           int32_t flush_mode);
  virtual ~ZlibTask();

  void OnDone() override;
  void OnWorkTick() override = 0;

  inline Immortal* immortal() { return _immortal; }
  inline ResizableBuffer* out() { return &_out_buffer; }
  inline size_t byte_length() { return _input.byte_length(); }
  inline void* data() { return _input.data(); }

 protected:
  void Prepare();
  void Done(bool all_done);
  void ConcatChunkToResult(ZlibStreamPtr stream, const ResizableBuffer& chunk);

  ConvenientArrayBufferStore _input;
  Immortal* _immortal;
  bool _prepared;
  ZlibWrapper* _parent;
  v8::Global<v8::Object> _life_object;

  ResizableBuffer _out_buffer;
  int32_t _flush_mode;

  using MacroTask::Done;
};

}  // namespace zlib
}  // namespace aworker

#endif  // SRC_ZLIB_ZLIB_TASK_H_
