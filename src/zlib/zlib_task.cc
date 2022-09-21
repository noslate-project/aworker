#include <memory>

#include "debug_utils.h"
#include "error_handling.h"
#include "zlib_task.h"
#include "zlib_wrapper.h"

namespace aworker {
namespace zlib {

using per_process::Debug;
using std::shared_ptr;
using v8::ArrayBuffer;
using v8::BackingStore;
using v8::Function;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Value;

ZlibTask::ZlibTask(Immortal* immortal,
                   ZlibWrapper* wrapper,
                   Local<Object> life_object,
                   Local<ArrayBuffer> array_buffer,
                   size_t array_buffer_offset,
                   size_t array_buffer_length,
                   int32_t flush_mode)
    : MacroTask(),
      _input(array_buffer, array_buffer_offset, array_buffer_length),
      _immortal(immortal),
      _prepared(false),
      _parent(wrapper),
      _flush_mode(flush_mode) {
  Isolate* isolate = immortal->isolate();

  // detach array_buffer
  array_buffer->Detach();

  _life_object.Reset(isolate, life_object);
}

ZlibTask::~ZlibTask() {
  _life_object.Reset();
}

void ZlibTask::OnDone() {
  Isolate* isolate = immortal()->isolate();
  HandleScope scope(isolate);

  if (has_error()) {
    Local<Value> error = GenException(isolate, last_error().c_str());
    Local<Value> argv[] = {error};
    if (!_parent->ended()) {
      _parent->OccurError(last_error().c_str());
    }
    _parent->DoTaskCallback(this, 1, argv);
  } else {
    ReleasedResizableBuffer ret = out()->Release();
    shared_ptr<BackingStore> backing_store = ArrayBuffer::NewBackingStore(
        ret.buff,
        ret.byte_length,
        [](void* data, size_t length, void* deleter_data) {
          free(deleter_data);
        },
        ret.buff);
    Local<ArrayBuffer> array_buffer = ArrayBuffer::New(isolate, backing_store);
    Local<Value> argv[] = {Undefined(isolate), array_buffer};
    _parent->DoTaskCallback(this, 2, argv);
  }

  _parent->TryTriggerNextTask();
}

void ZlibTask::Prepare() {
  ZlibStreamPtr stream = _parent->stream();

  stream->avail_in = _input.byte_length();
  stream->next_in = static_cast<unsigned char*>(_input.data());

  _prepared = true;
}

void ZlibTask::Done(bool all_done) {
  Debug(
      DebugCategory::ZLIB, "[trace] ZlibTask::Done(all_done: %d)\n", all_done);
  if (all_done) _parent->End();
  MacroTask::Done();
}

void ZlibTask::ConcatChunkToResult(ZlibStreamPtr stream,
                                   const ResizableBuffer& chunk) {
  CHECK_GE(chunk.byte_length(), stream->avail_out);
  size_t n = chunk.byte_length() - stream->avail_out;
  per_process::Debug(DebugCategory::ZLIB,
                     "[record] ZlibTask::ConcatChunkToResult(stream, chunk): "
                     "chunk length: %d\n",
                     n);

  if (n == 0) {
    return;
  }

  _out_buffer.Concat(chunk, n);
}

}  // namespace zlib
}  // namespace aworker
