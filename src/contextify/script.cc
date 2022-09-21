#include "script.h"
#include "util.h"

namespace aworker {
namespace contextify {

using v8::ArrayBuffer;
using v8::BackingStore;
using v8::Context;
using v8::FunctionTemplate;
using v8::Global;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::ObjectTemplate;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::String;
using v8::TryCatch;
using v8::Uint8Array;
using v8::UnboundScript;
using v8::Value;

const WrapperTypeInfo ScriptWrap::wrapper_type_info_{
    "script_wrap_name",
};

ScriptWrap::ScriptWrap(Immortal* immortal,
                       Local<Object> object,
                       Local<v8::UnboundScript> script)
    : BaseObject(immortal, object) {
  Isolate* isolate = immortal->isolate();
  MakeWeak();
  script_.Reset(isolate, script);
}

AWORKER_METHOD(ScriptWrap::New) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  Local<String> code = info[0].As<String>();
  Local<String> filename = info[1].As<String>();
  int line_offset = info[2].As<Int32>()->Value();
  int column_offset = info[3].As<Int32>()->Value();
  Local<Value> maybe_cached_data_buffer = info[4];

  ScriptOrigin origin(isolate,
                      filename,
                      line_offset,     // line offset
                      column_offset,   // column offset
                      true,            // is cross origin
                      -1,              // script id
                      Local<Value>(),  // source map URL
                      false,           // is opaque (?)
                      false,           // is wasm
                      false);          // is module

  bool has_cached_data = maybe_cached_data_buffer->IsUint8Array();
  ScriptCompiler::CompileOptions compile_options =
      has_cached_data ? ScriptCompiler::kConsumeCodeCache
                      : ScriptCompiler::kEagerCompile;

  std::unique_ptr<ScriptCompiler::CachedData> cached_data;
  if (has_cached_data) {
    std::shared_ptr<v8::BackingStore> cached_data_buffer =
        maybe_cached_data_buffer.As<Uint8Array>()->Buffer()->GetBackingStore();
    cached_data = std::make_unique<ScriptCompiler::CachedData>(
        static_cast<const uint8_t*>(cached_data_buffer->Data()),
        cached_data_buffer->ByteLength());
  }

  ScriptCompiler::Source source(code, origin, cached_data.release());

  MaybeLocal<UnboundScript> v8_script =
      ScriptCompiler::CompileUnboundScript(isolate, &source, compile_options);
  if (v8_script.IsEmpty()) {
    return;
  }

  Local<Context> context = immortal->context();
  bool cached_data_consumed = false;
  if (has_cached_data) {
    cached_data_consumed = !source.GetCachedData()->rejected;
  }
  info.This()
      ->Set(context,
            OneByteString(isolate, "cachedDataConsumed"),
            cached_data_consumed ? v8::True(isolate) : v8::False(isolate))
      .Check();

  new ScriptWrap(immortal, info.This(), v8_script.ToLocalChecked());

  info.GetReturnValue().Set(info.This());
}

AWORKER_METHOD(ScriptWrap::CreateCachedData) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  ScriptWrap* script;
  ASSIGN_OR_RETURN_UNWRAP(&script, info.This());

  std::unique_ptr<ScriptCompiler::CachedData> cached_data(
      ScriptCompiler::CreateCodeCache(script->script()));

  Local<ArrayBuffer> cached_data_buffer;
  // ASM module, etc.
  if (cached_data == nullptr) {
    cached_data_buffer = ArrayBuffer::New(isolate, cached_data->length);
  } else {
    cached_data_buffer = ArrayBuffer::New(isolate, cached_data->length);
    std::shared_ptr<BackingStore> backing_store =
        cached_data_buffer->GetBackingStore();
    memcpy(backing_store->Data(), cached_data->data, cached_data->length);
  }
  Local<Uint8Array> data_view =
      Uint8Array::New(cached_data_buffer, 0, cached_data_buffer->ByteLength());
  info.GetReturnValue().Set(data_view);
}

AWORKER_BINDING(ScriptInit) {
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, ScriptWrap::New);
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  MaybeLocal<String> maybe_name = String::NewFromUtf8(isolate, "ScriptWrap");
  CHECK(!maybe_name.IsEmpty());
  Local<String> name = maybe_name.ToLocalChecked();

  Local<ObjectTemplate> prototype_template = tpl->PrototypeTemplate();
  immortal->SetFunctionProperty(
      prototype_template, "createCachedData", ScriptWrap::CreateCachedData);

  tpl->SetClassName(name);

  exports->Set(context, name, tpl->GetFunction(context).ToLocalChecked())
      .Check();
}

AWORKER_EXTERNAL_REFERENCE(ScriptInit) {
  registry->Register(ScriptWrap::New);
  registry->Register(ScriptWrap::CreateCachedData);
}

}  // namespace contextify
}  // namespace aworker
