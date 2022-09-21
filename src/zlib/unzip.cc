#include "unzip.h"
#include "debug_utils.h"
#include "error_handling.h"
#include "unzip_task.h"

namespace aworker {
namespace zlib {

#define GZIP_HEADER_ID1 0x1f
#define GZIP_HEADER_ID2 0x8b

using per_process::Debug;
using std::shared_ptr;
using v8::ArrayBuffer;
using v8::Function;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::Value;

const WrapperTypeInfo UnzipWrapper::wrapper_type_info_{"unzip_wrapper"};

UnzipWrapper::UnzipWrapper(Immortal* immortal,
                           Local<Object> handle,
                           UnzipType type,
                           bool is_sync,
                           int window_bits,
                           Local<Value> _)
    : ZlibWrapper(
          immortal, handle, static_cast<int>(type), is_sync, window_bits, _),
      _first_two_bytes_parsed_status(
          UnzipFirstTwoBytesParsedStatus::kNonParsed),
      _head_task(nullptr) {}

UnzipWrapper::~UnzipWrapper() {
  if (_inited && !_ended) {
    DoZlibEnd();
    _ended = true;
  }
}

void UnzipWrapper::DoZlibEnd() {
  int ret = inflateEnd(&_stream);
  Debug(DebugCategory::ZLIB, "[call] inflateEnd(&_stream): %d\n", ret);
  CHECK(ret == Z_OK || ret == Z_DATA_ERROR);
}

bool UnzipWrapper::Init() {
  _stream.zalloc = Z_NULL;
  _stream.zfree = Z_NULL;
  _stream.opaque = Z_NULL;
  _stream.avail_in = 0;
  _stream.next_in = Z_NULL;

  int window_bits = _window_bits;
  if (_type == static_cast<int>(UnzipType::GUNZIP)) {
    window_bits += 16;
  }

  int ret = inflateInit2(&_stream, window_bits);
  _inited = true;

  Debug(DebugCategory::ZLIB,
        "[call] inflateInit2(&_stream, _window_bits: %d): %d\n",
        _window_bits,
        ret);

  return ret == Z_OK;
}

MaybeLocal<Value> UnzipWrapper::Push(Local<Object> this_object,
                                     Local<ArrayBuffer> array_buffer,
                                     size_t array_buffer_offset,
                                     size_t array_buffer_length,
                                     int32_t flush_mode,
                                     Local<Function> callback) {
  Immortal* immortal = this->immortal();
  std::unique_ptr<UnzipTask> task =
      std::make_unique<UnzipTask>(immortal,
                                  this,
                                  this_object,
                                  array_buffer,
                                  array_buffer_offset,
                                  array_buffer_length,
                                  flush_mode);

  Isolate* isolate = immortal->isolate();
  if (!task->byte_length()) {
    return ReturnEmptyResultSyncOrAsync(isolate, callback, task.get());
  }

  if (type<UnzipType>() == UnzipType::UNZIP) {
    int nread = 0;
    switch (_first_two_bytes_parsed_status) {
      case UnzipFirstTwoBytesParsedStatus::kNonParsed: {
        if (static_cast<unsigned char*>(task->data())[0] == GZIP_HEADER_ID1) {
          _first_two_bytes_parsed_status =
              UnzipFirstTwoBytesParsedStatus::kFirstParsed;
        } else {
          _type = static_cast<int>(UnzipType::INFLATE);
          break;
        }

        if (task->byte_length() == 1) {
          _head_task = std::move(task);
          return ReturnEmptyResultSyncOrAsync(
              isolate, callback, _head_task.get());
        } else {
          nread++;
          // fallthrough
        }
      }

      // 1. 这一块本来就到 1 了
      // 2. 从刚才的 0 里面没有 break 跳过来的
      case UnzipFirstTwoBytesParsedStatus::kFirstParsed: {
        if (static_cast<unsigned char*>(task->data())[nread] ==
            GZIP_HEADER_ID2) {
          _type = static_cast<int>(UnzipType::GUNZIP);
        } else {
          _type = static_cast<int>(UnzipType::INFLATE);
        }

        // 下面这句代码其实是冗余的，因为这个 `status` 再后续不再会被用到。
        _first_two_bytes_parsed_status =
            UnzipFirstTwoBytesParsedStatus::kSecondParsed;
        break;
      }

      // kSecondParsed 时，_type 已定，不可能再是 UnzipType::UNZIP，所以走到这里
      // 的逻辑肯定是错的。
      case UnzipFirstTwoBytesParsedStatus::kSecondParsed:
      default:
        UNREACHABLE("Broken unzip header logic");
    }
  }

  if (!_inited && !Init()) {
    ThrowException(isolate, "inflateInit() failed.");
    return MaybeLocal<Value>();
  }

  if (_is_sync) {
    // Consider js code:
    //
    // ```js
    // writer.write(<ArrayBuffer 0x1f>);
    // writer.write(<ArrayBuffer 0x??>);
    // ```
    //
    // This situation will generate two tasks, and when `write` first byte, the
    // task won't be executed. It will wait for the second byte:
    //
    //   1. The second byte is 0x8b: using GUNZIP;
    //   2. The second byte is not 0x8b: using INFLATE.
    //
    // And then run task 1 immediately followed by task 2.
    //
    // `_head_task` means `task 1`, and `backing_store_1` is for storing result
    // of `_head_task`.
    //
    // `backing_store_2` is for non-`_head_task`.
    shared_ptr<v8::BackingStore> backing_store_1 = nullptr;
    shared_ptr<v8::BackingStore> backing_store_2 = nullptr;
    Local<ArrayBuffer> array_buffer_1;
    Local<ArrayBuffer> array_buffer_2;

    if (_head_task) {
      while (!_head_task->is_done() && !_head_task->has_error()) {
        _head_task->OnWorkTick();
      }

      if (_head_task->has_error()) {
        OccurError(_head_task->last_error().c_str());
        ThrowException(isolate, task->last_error().c_str());
        _head_task = nullptr;
        return MaybeLocal<Value>();
      }

      ReleasedResizableBuffer managed = task->out()->Release();
      backing_store_1 = ArrayBuffer::NewBackingStore(
          managed.buff,
          managed.byte_length,
          [](void* data, size_t length, void* deleter_data) {
            free(deleter_data);
          },
          managed.buff);
      array_buffer_1 = ArrayBuffer::New(isolate, backing_store_1);
      _head_task = nullptr;
    }

    while (!task->is_done() && !task->has_error()) {
      task->OnWorkTick();
    }

    if (task->has_error()) {
      if (!ended()) OccurError(task->last_error().c_str());
      ThrowException(isolate, task->last_error().c_str());
      return MaybeLocal<Value>();
    }

    ReleasedResizableBuffer managed = task->out()->Release();
    backing_store_2 = ArrayBuffer::NewBackingStore(
        managed.buff,
        managed.byte_length,
        [](void* data, size_t length, void* deleter_data) {
          free(deleter_data);
        },
        managed.buff);
    array_buffer_2 = ArrayBuffer::New(isolate, backing_store_2);

    if (backing_store_1 != nullptr) {
      Local<Value> values[] = {array_buffer_1, array_buffer_2};
      Local<v8::Array> array = v8::Array::New(isolate, values, 2);
      return array;
    }

    return array_buffer_2;
  }

  _callback_map[task.get()].Reset(isolate, callback);
  if (_head_task) {
    _task_queue.push(std::move(_head_task));
  }
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
