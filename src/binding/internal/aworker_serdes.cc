#include <utility>
#include "aworker_binding.h"
#include "immortal.h"

namespace aworker {
namespace serdes {

using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::String;
using v8::Value;
using v8::ValueDeserializer;
using v8::ValueSerializer;

class ValueSerializerDelegate : public ValueSerializer::Delegate {
 public:
  explicit ValueSerializerDelegate(Immortal* immortal) : _immortal(immortal) {}

  AWORKER_DISALLOW_ASSIGN_COPY(ValueSerializerDelegate)

  void ThrowDataCloneError(Local<String> message) override {
    Isolate* isolate = _immortal->isolate();
    HandleScope scope(isolate);
    Local<Value> argv[2] = {message, OneByteString(isolate, "DataCloneError")};
    Local<Value> exception = _immortal->dom_exception_constructor()
                                 ->NewInstance(_immortal->context(), 2, argv)
                                 .ToLocalChecked();
    isolate->ThrowException(exception);
  }

 private:
  Immortal* _immortal;
};

AWORKER_METHOD(StructuredClone) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal->context();
  ValueSerializerDelegate delegate(immortal);
  std::pair<uint8_t*, size_t> buffer{nullptr, 0};

  {
    ValueSerializer ser(isolate, &delegate);
    Maybe<bool> result = ser.WriteValue(context, info[0]);
    if (!result.FromMaybe(false)) {
      return;
    }
    buffer = ser.Release();
  }

  ValueDeserializer des(isolate, buffer.first, buffer.second);
  MaybeLocal<Value> val = des.ReadValue(context);
  if (!val.IsEmpty()) {
    info.GetReturnValue().Set(val.ToLocalChecked());
  }

  delegate.FreeBufferMemory(buffer.first);
}

AWORKER_BINDING(Init) {
  immortal->SetFunctionProperty(exports, "structuredClone", StructuredClone);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  registry->Register(StructuredClone);
}

}  // namespace serdes
}  // namespace aworker

AWORKER_BINDING_REGISTER(serdes, aworker::serdes::Init, aworker::serdes::Init)
