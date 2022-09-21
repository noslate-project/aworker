#include "aworker_binding.h"
#include "immortal.h"

using v8::Array;
using v8::ArrayBuffer;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::Promise;
using v8::Value;

namespace aworker {
namespace types {

#define VALUE_METHOD_MAP(V)                                                    \
  V(External)                                                                  \
  V(Date)                                                                      \
  V(ArgumentsObject)                                                           \
  V(BooleanObject)                                                             \
  V(NumberObject)                                                              \
  V(StringObject)                                                              \
  V(SymbolObject)                                                              \
  V(NativeError)                                                               \
  V(RegExp)                                                                    \
  V(AsyncFunction)                                                             \
  V(GeneratorFunction)                                                         \
  V(GeneratorObject)                                                           \
  V(Promise)                                                                   \
  V(Map)                                                                       \
  V(Set)                                                                       \
  V(MapIterator)                                                               \
  V(SetIterator)                                                               \
  V(WeakMap)                                                                   \
  V(WeakSet)                                                                   \
  V(ArrayBuffer)                                                               \
  V(TypedArray)                                                                \
  V(Uint8Array)                                                                \
  V(Uint8ClampedArray)                                                         \
  V(Uint16Array)                                                               \
  V(Uint32Array)                                                               \
  V(Int8Array)                                                                 \
  V(Int16Array)                                                                \
  V(Int32Array)                                                                \
  V(Float32Array)                                                              \
  V(Float64Array)                                                              \
  V(BigInt64Array)                                                             \
  V(BigUint64Array)                                                            \
  V(DataView)                                                                  \
  V(SharedArrayBuffer)                                                         \
  V(Proxy)                                                                     \
  /* V(WebAssemblyCompiledModule)                                        \ */  \
  V(ModuleNamespaceObject)

#define V(type)                                                                \
  AWORKER_METHOD(Is##type) { info.GetReturnValue().Set(info[0]->Is##type()); }

VALUE_METHOD_MAP(V)
#undef V

AWORKER_METHOD(IsAnyArrayBuffer) {
  info.GetReturnValue().Set(info[0]->IsArrayBuffer() ||
                            info[0]->IsSharedArrayBuffer());
}

AWORKER_METHOD(GetPromiseDetails) {
  // Return undefined if it's not a Promise.
  if (!info[0]->IsPromise()) return;

  Immortal* immortal = Immortal::GetCurrent(info);
  auto isolate = immortal->isolate();
  auto context = immortal->context();
  HandleScope scope(isolate);

  Local<Promise> promise = info[0].As<Promise>();
  Local<Array> ret = Array::New(isolate, 2);

  int state = promise->State();
  ret->Set(context, 0, Integer::New(isolate, state)).FromJust();
  if (state != Promise::PromiseState::kPending)
    ret->Set(context, 1, promise->Result()).FromJust();

  info.GetReturnValue().Set(ret);
}

AWORKER_METHOD(DetachArrayBuffer) {
  if (!info[0]->IsArrayBuffer()) {
    info.GetReturnValue().Set(false);
    return;
  }
  Local<ArrayBuffer> it = info[0].As<ArrayBuffer>();
  if (!it->IsDetachable()) {
    info.GetReturnValue().Set(false);
    return;
  }
  it->Detach();
  info.GetReturnValue().Set(true);
}

AWORKER_METHOD(IsArrayBufferDetached) {
  bool result = true;
  if (info[0]->IsArrayBuffer()) {
    result = !info[0].As<ArrayBuffer>()->IsDetachable();
  }
  info.GetReturnValue().Set(result);
}

AWORKER_BINDING(Init) {
#define V(type) immortal->SetFunctionProperty(exports, "is" #type, Is##type);
  VALUE_METHOD_MAP(V)
#undef V

  immortal->SetFunctionProperty(exports, "isAnyArrayBuffer", IsAnyArrayBuffer);
  immortal->SetFunctionProperty(
      exports, "getPromiseDetails", GetPromiseDetails);
  immortal->SetFunctionProperty(
      exports, "isArrayBufferDetached", IsArrayBufferDetached);
  immortal->SetFunctionProperty(
      exports, "detachArrayBuffer", DetachArrayBuffer);

  immortal->SetIntegerProperty(exports, "kPending", Promise::kPending);
  immortal->SetIntegerProperty(exports, "kRejected", Promise::kRejected);
  immortal->SetIntegerProperty(exports, "kFulfilled", Promise::kFulfilled);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
#define V(type) registry->Register(Is##type);
  VALUE_METHOD_MAP(V)
#undef V

  registry->Register(IsAnyArrayBuffer);
  registry->Register(GetPromiseDetails);
  registry->Register(IsArrayBufferDetached);
  registry->Register(DetachArrayBuffer);
}

}  // namespace types
}  // namespace aworker

AWORKER_BINDING_REGISTER(types, aworker::types::Init, aworker::types::Init)
