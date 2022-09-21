#include "zlib_wrapper.h"
#include "debug_utils.h"
#include "error_handling.h"
#include "unzip.h"
#include "util.h"
#include "zip.h"

namespace aworker {
namespace zlib {

#define Z_MIN_MEMLEVEL 1
#define Z_MAX_MEMLEVEL 9
#define Z_DEFAULT_MEMLEVEL 8
#define Z_MIN_LEVEL -1
#define Z_MAX_LEVEL 9
#define Z_DEFAULT_LEVEL Z_DEFAULT_COMPRESSION
#define Z_MIN_WINDOWBITS 8
#define Z_MAX_WINDOWBITS 15
#define Z_DEFAULT_WINDOWBITS 15

using v8::ArrayBuffer;
using v8::Boolean;
using v8::Context;
using v8::Function;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;
using v8::Value;

int pagesize = getpagesize();

template <class T, typename TypeEnum>
inline void ZlibWrapper::Init(Local<Object> exports,
                              const char* constructor_name) {
  Immortal* immortal = Immortal::GetCurrent(exports->CreationContext());

  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal->context();

  Local<FunctionTemplate> tpl =
      FunctionTemplate::New(isolate, New<T, TypeEnum>);
  tpl->Inherit(BaseObject::GetConstructorTemplate(immortal));
  tpl->InstanceTemplate()->SetInternalFieldCount(
      BaseObject::kInternalFieldCount);

  Local<String> name = aworker::OneByteString(isolate, constructor_name);

  tpl->SetClassName(name);

  Local<ObjectTemplate> prototype_template = tpl->PrototypeTemplate();
  immortal->SetFunctionProperty(prototype_template, "push", Push<T>);
  immortal->SetFunctionProperty(prototype_template, "isDone", IsDone<T>);
  immortal->SetFunctionProperty(prototype_template, "hasError", HasError<T>);
  immortal->SetFunctionProperty(
      prototype_template, "errorMessage", ErrorMessage<T>);

  exports->Set(context, name, tpl->GetFunction(context).ToLocalChecked())
      .Check();
}

template <class T, typename TypeEnum>
inline void ZlibWrapper::Init(ExternalReferenceRegistry* registry) {
  registry->Register(New<T, TypeEnum>);
  registry->Register(Push<T>);
  registry->Register(IsDone<T>);
  registry->Register(HasError<T>);
  registry->Register(ErrorMessage<T>);
}

template <class T, typename TypeEnum>
inline AWORKER_METHOD(ZlibWrapper::New) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  int type = info[0].As<Integer>()->Int32Value(immortal->context()).FromJust();
  if (type >= T::TypeEnumMax()) {
    ThrowException(isolate,
                   "Unsupported compression / uncompression type.",
                   ExceptionType::kRangeError);
    return;
  }
  bool is_sync = info[1].As<Boolean>()->IsTrue();
  int window_bits = info[2].As<Int32>()->Value();

  new T(immortal,
        info.This(),
        static_cast<TypeEnum>(type),
        is_sync,
        window_bits,
        info[3]);

  info.GetReturnValue().Set(info.This());
}

template <class T>
inline AWORKER_METHOD(ZlibWrapper::Push) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  Local<Context> context = immortal->context();
  HandleScope scope(isolate);

  T* wrapper;
  ASSIGN_OR_RETURN_UNWRAP(&wrapper, info.This());

  Local<ArrayBuffer> array_buffer = info[0].As<ArrayBuffer>();
  int32_t array_buffer_offset =
      info[1].As<Int32>()->Int32Value(context).FromJust();
  int32_t array_buffer_length =
      info[2].As<Int32>()->Int32Value(context).FromJust();
  int32_t flush_mode = info[3].As<Int32>()->Int32Value(context).FromJust();
  Local<Function> callback;
  if (!wrapper->_is_sync) {
    callback = info[4].As<Function>();
  }

  MaybeLocal<Value> result = wrapper->Push(info.This(),
                                           array_buffer,
                                           array_buffer_offset,
                                           array_buffer_length,
                                           flush_mode,
                                           callback);
  if (!result.IsEmpty()) {
    info.GetReturnValue().Set(result.ToLocalChecked());
  }
}

template <class T>
inline AWORKER_METHOD(ZlibWrapper::IsDone) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  T* wrapper;
  ASSIGN_OR_RETURN_UNWRAP(&wrapper, info.This());
  info.GetReturnValue().Set(wrapper->_finished);
}

template <class T>
inline AWORKER_METHOD(ZlibWrapper::HasError) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  T* wrapper;
  ASSIGN_OR_RETURN_UNWRAP(&wrapper, info.This());
  info.GetReturnValue().Set(wrapper->_errored);
}

template <class T>
inline AWORKER_METHOD(ZlibWrapper::ErrorMessage) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  T* wrapper;
  ASSIGN_OR_RETURN_UNWRAP(&wrapper, info.This());

  if (wrapper->_errored) {
    MaybeLocal<String> msg =
        String::NewFromUtf8(isolate, wrapper->_error.c_str());
    CHECK(!msg.IsEmpty());
    info.GetReturnValue().Set(msg.ToLocalChecked());
  } else {
    info.GetReturnValue().Set(Undefined(isolate));
  }
}

ZlibWrapper::ZlibWrapper(Immortal* immortal,
                         Local<Object> handle,
                         int type,
                         bool is_sync,
                         int window_bits,
                         Local<Value> options)
    : AsyncWrap(immortal, handle),
      _type(type),
      _is_sync(is_sync),
      _window_bits(window_bits),
      _task_queue_processed(0),
      _running(false),
      _errored(false),
      _finished(false),
      _error(""),
      _inited(false),
      _ended(false) {
  // TODO(chengzhong.wcz): clarify js-native object lifetime
  MakeWeak();

  memset(&_stream, 0, sizeof(_stream));

  _chunk_buffer.Realloc(pagesize);
  _stream.next_in = nullptr;
  _stream.next_out = static_cast<unsigned char*>(_chunk_buffer.buffer());
  _stream.avail_in = 0;
  _stream.avail_out = _chunk_buffer.byte_length();
}

MaybeLocal<Value> ZlibWrapper::ReturnEmptyResultSyncOrAsync(
    Isolate* isolate, Local<Function> callback, ZlibTask* task) {
  if (_is_sync) {
    ClearTaskCallback(task);
    return ArrayBuffer::New(isolate, 0);
  }

  Local<Value> argv[2] = {Undefined(isolate), ArrayBuffer::New(isolate, 0)};
  MakeCallback(callback, 2, argv);
  ClearTaskCallback(task);
  return MaybeLocal<Value>();
}

void ZlibWrapper::TryTriggerNextTask() {
  if (!_task_queue.empty()) {
    immortal()->macro_task_queue()->Enqueue(std::move(_task_queue.front()));
    _task_queue.pop();
  } else {
    _running = false;
  }
}

void ZlibWrapper::DoTaskCallback(const ZlibTask* key,
                                 int argc,
                                 Local<Value>* argv) {
  CallbackMap::iterator it = _callback_map.find(key);
  if (it == _callback_map.end()) return;
  if (it->second.IsEmpty()) {
    ClearTaskCallback(it);
    return;
  }

  Isolate* isolate = immortal()->isolate();
  HandleScope scope(isolate);

  Local<Function> callback = Local<Function>::New(isolate, it->second);
  ClearTaskCallback(it);

  MakeCallback(callback, argc, argv);
}

void ZlibWrapper::ClearTaskCallback(const ZlibTask* key) {
  ClearTaskCallback(_callback_map.find(key));
}

void ZlibWrapper::ClearTaskCallback(CallbackMap::iterator it) {
  if (it == _callback_map.end()) return;
  it->second.Reset();
  _callback_map.erase(it);
}

void ZlibWrapper::OccurError(const char* error) {
  _running = false;
  _errored = true;
  _error = error;
  _ended = true;
  DoZlibEnd();
}

void ZlibWrapper::End() {
  _running = false;
  _finished = true;
  _ended = true;
  DoZlibEnd();
}

AWORKER_BINDING(Init) {
  ZlibWrapper::Init<UnzipWrapper, UnzipType>(exports, "UnzipWrapper");
  ZlibWrapper::Init<ZipWrapper, ZipType>(exports, "ZipWrapper");

  Local<Object> flush = Object::New(immortal->isolate());
#define SET_FLUSH_MODE(name) immortal->SetIntegerProperty(flush, #name, name)
  SET_FLUSH_MODE(Z_NO_FLUSH);
  SET_FLUSH_MODE(Z_PARTIAL_FLUSH);
  SET_FLUSH_MODE(Z_SYNC_FLUSH);
  SET_FLUSH_MODE(Z_FULL_FLUSH);
  SET_FLUSH_MODE(Z_FINISH);
  SET_FLUSH_MODE(Z_BLOCK);
  SET_FLUSH_MODE(Z_TREES);
  immortal->SetValueProperty(exports, "FLUSH_MODE", flush);

  Local<Object> window_bits = Object::New(immortal->isolate());
#define SET_WINDOW_BITS(name)                                                  \
  immortal->SetIntegerProperty(window_bits, #name, name)
  SET_WINDOW_BITS(Z_MIN_WINDOWBITS);
  SET_WINDOW_BITS(Z_MAX_WINDOWBITS);
  SET_WINDOW_BITS(Z_DEFAULT_WINDOWBITS);
  immortal->SetValueProperty(exports, "WINDOWBITS", window_bits);

  Local<Object> level = Object::New(immortal->isolate());
#define SET_LEVEL(name) immortal->SetIntegerProperty(level, #name, name)
  SET_LEVEL(Z_MIN_LEVEL);
  SET_LEVEL(Z_MAX_LEVEL);
  SET_LEVEL(Z_DEFAULT_LEVEL);
  SET_LEVEL(Z_MIN_MEMLEVEL);
  SET_LEVEL(Z_MAX_MEMLEVEL);
  SET_LEVEL(Z_DEFAULT_MEMLEVEL);
  immortal->SetValueProperty(exports, "LEVEL", level);

  Local<Object> strategy = Object::New(immortal->isolate());
#define SET_STRATEGY(name) immortal->SetIntegerProperty(strategy, #name, name)
  SET_STRATEGY(Z_DEFAULT_STRATEGY);
  SET_STRATEGY(Z_FILTERED);
  SET_STRATEGY(Z_HUFFMAN_ONLY);
  SET_STRATEGY(Z_RLE);
  SET_STRATEGY(Z_FIXED);
  immortal->SetValueProperty(exports, "STRATEGY", strategy);

  Local<Object> uncompress_type = Object::New(immortal->isolate());
#define SET_UNCOMPRESS_TYPE(name)                                              \
  immortal->SetIntegerProperty(                                                \
      uncompress_type, #name, static_cast<int>(UnzipType::name))
  SET_UNCOMPRESS_TYPE(GUNZIP);
  SET_UNCOMPRESS_TYPE(INFLATE);
  SET_UNCOMPRESS_TYPE(UNZIP);
  immortal->SetValueProperty(exports, "UNCOMPRESS_TYPE", uncompress_type);

  Local<Object> compress_type = Object::New(immortal->isolate());
#define SET_COMPRESS_TYPE(name)                                                \
  immortal->SetIntegerProperty(                                                \
      compress_type, #name, static_cast<int>(ZipType::name))
  SET_COMPRESS_TYPE(GZIP);
  SET_COMPRESS_TYPE(DEFLATE);
  immortal->SetValueProperty(exports, "COMPRESS_TYPE", compress_type);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  ZlibWrapper::Init<UnzipWrapper, UnzipType>(registry);
  ZlibWrapper::Init<ZipWrapper, ZipType>(registry);
}

}  // namespace zlib
}  // namespace aworker

AWORKER_BINDING_REGISTER(zlib, aworker::zlib::Init, aworker::zlib::Init)
