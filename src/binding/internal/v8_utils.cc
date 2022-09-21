#include <cwalk.h>
#include <v8-profiler.h>

#include "aworker_binding.h"
#include "error_handling.h"
#include "immortal.h"

namespace aworker {

using v8::HandleScope;
using v8::HeapSnapshot;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::String;

namespace {
class FileOutputStream : public v8::OutputStream {
 public:
  explicit FileOutputStream(FILE* stream) : _stream(stream) {}

  int GetChunkSize() override { return 65536; }

  void EndOfStream() override {}

  WriteResult WriteAsciiChunk(char* data, int size) override {
    const size_t len = static_cast<size_t>(size);
    size_t off = 0;

    while (off < len && !feof(_stream) && !ferror(_stream))
      off += fwrite(data + off, 1, len - off, _stream);

    return off == len ? kContinue : kAbort;
  }

 private:
  FILE* _stream;
};

void DeleteHeapSnapshot(const HeapSnapshot* snapshot) {
  const_cast<HeapSnapshot*>(snapshot)->Delete();
}
using HeapSnapshotPointer =
    DeleteFnPtr<const v8::HeapSnapshot, DeleteHeapSnapshot>;

}  // namespace

namespace v8_utils {
AWORKER_METHOD(WriteHeapSnapshot) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  String::Utf8Value filename(isolate, info[0]);

  FILE* fp = fopen(*filename, "w+");
  if (fp == nullptr) {
    char message[256];
    snprintf(message, sizeof(message), "Cannot open file %s.", *filename);
    ThrowException(isolate, message);
    return;
  }

  HeapSnapshotPointer snapshot{isolate->GetHeapProfiler()->TakeHeapSnapshot()};
  if (!snapshot) {
    fclose(fp);
    fp = nullptr;
    ThrowException(isolate, "Fail to create heapsnapshot due to low memory.");
    return;
  }

  FileOutputStream stream(fp);
  snapshot->Serialize(&stream, HeapSnapshot::kJSON);
  fclose(fp);
  fp = nullptr;
}

// TODO(kaidi.zkd): remove me.
AWORKER_METHOD(GetTotalHeapSize) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  v8::HeapStatistics stat;
  isolate->GetHeapStatistics(&stat);

  Local<v8::Integer> ret = v8::Integer::New(isolate, stat.total_heap_size());
  info.GetReturnValue().Set(ret);
}

AWORKER_BINDING(Init) {
  immortal->SetFunctionProperty(
      exports, "writeHeapSnapshot", WriteHeapSnapshot);

  immortal->SetFunctionProperty(exports, "getTotalHeapSize", GetTotalHeapSize);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  registry->Register(WriteHeapSnapshot);
  registry->Register(GetTotalHeapSize);
}

}  // namespace v8_utils
}  // namespace aworker

AWORKER_BINDING_REGISTER(v8, aworker::v8_utils::Init, aworker::v8_utils::Init)
