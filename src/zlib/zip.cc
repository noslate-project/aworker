#include "zip.h"
#include "debug_utils.h"
#include "error_handling.h"
#include "zip_task.h"

namespace aworker {
namespace zlib {

using per_process::Debug;
using std::shared_ptr;
using v8::ArrayBuffer;
using v8::Context;
using v8::Function;
using v8::HandleScope;
using v8::Int32;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::Value;

const WrapperTypeInfo ZipWrapper::wrapper_type_info_{"zip_wrapper"};

ZipWrapper::ZipWrapper(Immortal* immortal,
                       Local<Object> handle,
                       ZipType type,
                       bool is_sync,
                       int window_bits,
                       Local<Value> options)
    : ZlibWrapper(immortal,
                  handle,
                  static_cast<int>(type),
                  is_sync,
                  window_bits,
                  options) {
  CHECK(options->IsObject() && !options.IsEmpty());
  Isolate* isolate = immortal->isolate();
  Local<Context> context = immortal->context();
  HandleScope scope(isolate);

  MaybeLocal<Object> maybe_options_object = options->ToObject(context);
  CHECK(!maybe_options_object.IsEmpty());
  Local<Object> options_object = maybe_options_object.ToLocalChecked();

#define SET_PROPERTY(name)                                                     \
  {                                                                            \
    MaybeLocal<Value> maybe_##name =                                           \
        options_object->Get(context, immortal->name##_string());               \
    CHECK(!maybe_##name.IsEmpty());                                            \
    Local<Value> name##_value = maybe_##name.ToLocalChecked();                 \
    CHECK(name##_value->IsInt32());                                            \
    MaybeLocal<Int32> maybe_name##_int32 = name##_value->ToInt32(context);     \
    CHECK(!maybe_name##_int32.IsEmpty());                                      \
    _##name = maybe_name##_int32.ToLocalChecked()->Value();                    \
  }

  SET_PROPERTY(level);
  SET_PROPERTY(mem_level);
  SET_PROPERTY(strategy);

#undef SET_PROPERTY
}

ZipWrapper::~ZipWrapper() {
  if (_inited && !_ended) {
    DoZlibEnd();
    _ended = true;
  }

  // clear task callbacks
  for (auto it = _callback_map.begin(); it != _callback_map.end(); it++) {
    it->second.Reset();
  }
  _callback_map.clear();
}

void ZipWrapper::DoZlibEnd() {
  int ret = deflateEnd(&_stream);
  Debug(DebugCategory::ZLIB, "[call] deflateEnd(&_stream): %d\n", ret);
  CHECK(ret == Z_OK || ret == Z_DATA_ERROR);
}

bool ZipWrapper::Init() {
  _stream.zalloc = Z_NULL;
  _stream.zfree = Z_NULL;
  _stream.opaque = Z_NULL;
  _stream.avail_in = 0;
  _stream.next_in = Z_NULL;

  int window_bits = _window_bits;
  if (_type == static_cast<int>(ZipType::GZIP)) {
    window_bits += 16;
  }

  int ret = deflateInit2(
      &_stream, _level, Z_DEFLATED, window_bits, _mem_level, _strategy);

  Debug(
      DebugCategory::ZLIB,
      "[call] deflateInit2(&_stream, _level: %d, Z_DEFLATED, _window_bits: %d, "
      "_mem_level: %d, _strategy: %d): %d\n",
      _level,
      _window_bits,
      _mem_level,
      _strategy,
      ret);
  _inited = true;
  return ret == Z_OK;
}

MaybeLocal<Value> ZipWrapper::Push(Local<Object> this_object,
                                   Local<ArrayBuffer> array_buffer,
                                   size_t array_buffer_offset,
                                   size_t array_buffer_length,
                                   int32_t flush_mode,
                                   Local<Function> callback) {
  Immortal* immortal = this->immortal();
  std::unique_ptr<ZipTask> task = std::make_unique<ZipTask>(immortal,
                                                            this,
                                                            this_object,
                                                            array_buffer,
                                                            array_buffer_offset,
                                                            array_buffer_length,
                                                            flush_mode);

  Isolate* isolate = immortal->isolate();
  if (!task->byte_length() && flush_mode != Z_FINISH) {
    return ReturnEmptyResultSyncOrAsync(isolate, callback, task.get());
  }

  if (!_inited && !Init()) {
    ThrowException(isolate, "deflateInit() failed.");
    return MaybeLocal<Value>();
  }

  if (_is_sync) {
    shared_ptr<v8::BackingStore> backing_store = nullptr;

    while (!task->is_done() && !task->has_error()) {
      task->OnWorkTick();
    }

    if (task->has_error()) {
      if (!ended()) OccurError(task->last_error().c_str());
      ThrowException(isolate, task->last_error().c_str());
      return MaybeLocal<Value>();
    }

    ReleasedResizableBuffer managed = task->out()->Release();
    backing_store = ArrayBuffer::NewBackingStore(
        managed.buff,
        managed.byte_length,
        [](void* data, size_t length, void* deleter_data) {
          free(deleter_data);
        },
        managed.buff);
    return ArrayBuffer::New(isolate, backing_store);
  }

  _callback_map[task.get()].Reset(isolate, callback);
  _task_queue.push(std::move(task));

  if (!_running) {
    _running = true;
    immortal->macro_task_queue()->Enqueue(std::move(_task_queue.front()));
    _task_queue.pop();
  }

  return MaybeLocal<Value>();
}

}  // namespace zlib
}  // namespace aworker
