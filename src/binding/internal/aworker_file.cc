#include "aworker_binding.h"
#include "debug_utils.h"
#include "immortal.h"
#include "util.h"

namespace aworker {

using v8::ArrayBuffer;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;

AWORKER_METHOD(ReadFile) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  Local<String> filename = info[0].As<String>();
  String::Utf8Value v8_filename(isolate, filename);

  // Ownership will be moved to BackingStore.
  std::string* content = new std::string();
  int r = ReadFileSync(content, *v8_filename);
  if (r != 0) {
    delete content;
    const std::string buf = SPrintF("Cannot read file. %s: %s \"%s\"",
                                    uv_err_name(r),
                                    uv_strerror(r),
                                    *v8_filename);
    Local<String> message = OneByteString(isolate, buf.c_str());
    isolate->ThrowException(v8::Exception::Error(message));
    return;
  }

  std::shared_ptr<v8::BackingStore> backing_store =
      ArrayBuffer::NewBackingStore(
          const_cast<char*>(content->c_str()),
          content->size(),
          [](void* data, size_t length, void* deleter_data) {
            std::string* content = reinterpret_cast<std::string*>(deleter_data);
            delete content;
          },
          content);
  Local<ArrayBuffer> array_buffer = ArrayBuffer::New(isolate, backing_store);
  info.GetReturnValue().Set(array_buffer);
}

namespace file {
AWORKER_BINDING(Init) {
  immortal->SetFunctionProperty(exports, "readFile", ReadFile);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  registry->Register(ReadFile);
}

}  // namespace file

}  // namespace aworker

AWORKER_BINDING_REGISTER(file, aworker::file::Init, aworker::file::Init)
