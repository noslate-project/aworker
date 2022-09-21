#ifndef SRC_ZLIB_ZLIB_WRAPPER_H_
#define SRC_ZLIB_ZLIB_WRAPPER_H_

extern "C" {
#include <zlib.h>
}

#include <map>

#include "async_wrap.h"
#include "aworker_binding.h"
#include "zlib_task.h"

namespace aworker {
namespace zlib {

using ZlibStream = z_stream;
using CallbackMap = std::map<const ZlibTask*, v8::Global<v8::Function>>;

class ZlibWrapper : public AsyncWrap {
 public:
  template <class T, typename TypeEnum>
  static void Init(v8::Local<v8::Object> exports, const char* constructor_name);
  template <class T, typename TypeEnum>
  static void Init(ExternalReferenceRegistry* registry);
  template <class T, typename TypeEnum>
  static AWORKER_METHOD(New);
  template <class T>
  static AWORKER_METHOD(Push);
  template <class T>
  static AWORKER_METHOD(IsDone);
  template <class T>
  static AWORKER_METHOD(HasError);
  template <class T>
  static AWORKER_METHOD(ErrorMessage);

  static int TypeEnumMax() { return 0; }

 public:
  ZlibWrapper(Immortal* immortal,
              v8::Local<v8::Object> handle,
              int type,
              bool is_sync,
              int window_bits,
              v8::Local<v8::Value> options);
  virtual ~ZlibWrapper() {}

  v8::MaybeLocal<v8::Value> ReturnEmptyResultSyncOrAsync(
      v8::Isolate* isolate, v8::Local<v8::Function> callback, ZlibTask* task);
  void TryTriggerNextTask();

  virtual bool Init() = 0;
  virtual v8::MaybeLocal<v8::Value> Push(
      v8::Local<v8::Object> this_object,
      v8::Local<v8::ArrayBuffer> array_buffer,
      size_t array_buffer_offset,
      size_t array_buffer_length,
      int32_t flush_mode,
      v8::Local<v8::Function> callback) = 0;

  void DoTaskCallback(const ZlibTask* key,
                      int argc,
                      v8::Local<v8::Value>* argv);
  void ClearTaskCallback(const ZlibTask* key);
  void ClearTaskCallback(CallbackMap::iterator it);

  void OccurError(const char* error = nullptr);
  void End();
  virtual void DoZlibEnd() = 0;

  template <typename T>
  inline T type() {
    return static_cast<T>(_type);
  }
  inline ZlibStreamPtr stream() { return &_stream; }
  inline bool finished() { return _finished; }
  inline bool errored() { return _errored; }
  inline std::string error() { return _error; }
  inline bool ended() { return _ended; }

  /**
   * All tasks belong to same `ZlibWrapper` should use the same chunk buffer to
   * reduce frequent memory creation / release. So here's a
   * `ZlibWrapper::chunk_buffer()` for all tasks belong to current
   * `ZlibWrapper`.
   */
  inline ResizableBuffer* chunk_buffer() { return &_chunk_buffer; }

 protected:
  ResizableBuffer _chunk_buffer;
  int _type;
  bool _is_sync;
  int _window_bits;
  ZlibStream _stream;
  std::queue<std::unique_ptr<ZlibTask>> _task_queue;
  CallbackMap _callback_map;

  int _task_queue_processed;
  bool _running;
  bool _errored;
  bool _finished;
  std::string _error;
  bool _inited;
  bool _ended;
};

}  // namespace zlib
}  // namespace aworker

#endif  // SRC_ZLIB_ZLIB_WRAPPER_H_
