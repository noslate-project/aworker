#include <iostream>

#include "aworker_platform.h"
#include "error_handling.h"
#include "immortal.h"

#include "aworker_i18n.h"
#include "debug_utils-inl.h"
#include "utils/resizable_buffer.h"

namespace aworker {

namespace i18n {

using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BigInt;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Isolate;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;
using v8::Uint32Array;
using v8::Uint8Array;

const WrapperTypeInfo ConverterObject::wrapper_type_info_{
    "converter_object",
};

ConverterObject::ConverterObject(Immortal* immortal,
                                 Local<Object> object,
                                 Local<String> name)
    : SnapshotableObject(immortal, object), conv_(nullptr) {
  object->SetInternalField(ConverterObject::kName, name);
  MakeWeak();
}

ConverterObject::~ConverterObject() {
  if (conv_) {
    ucnv_close(conv_);
  }
}

EmbedderFieldObjectType ConverterObject::embedder_field_object_type() {
  return EmbedderFieldObjectType::kConverterObject;
}

EmbedderFieldStartupData* ConverterObject::Serialize(int internal_field_index) {
  if (internal_field_index != BaseObject::kSlot) {
    return nullptr;
  }
  return EmbedderFieldStartupData::New(
      EmbedderFieldObjectType::kConverterObject);
}

void ConverterObject::Deserialize(v8::Local<v8::Context> context,
                                  v8::Local<v8::Object> holder,
                                  int internal_field_index,
                                  const EmbedderFieldStartupData* info) {
  if (internal_field_index != BaseObject::kSlot) {
    return;
  }
  Immortal* immortal = Immortal::GetCurrent(context);
  Local<v8::String> name =
      holder->GetInternalField(ConverterObject::kName).As<String>();
  ConverterObject* co = new ConverterObject(immortal, holder, name);

  String::Utf8Value utf8_name(immortal->isolate(), name);
  UErrorCode status = U_ZERO_ERROR;
  co->conv_ = ucnv_open(*utf8_name, &status);
  CHECK(!U_FAILURE(status));
}

size_t ConverterObject::min_char_size() const {
  return ucnv_getMinCharSize(conv_);
}

AWORKER_METHOD(ConverterObject::New) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  Local<Context> context = immortal->context();
  Local<Object> obj;

  String::Utf8Value name(isolate, info[0]);
  uint32_t flags = info[1]->Uint32Value(context).ToChecked();

  bool fatal = (flags & CONVERTER_FLAGS_FATAL) == CONVERTER_FLAGS_FATAL;

  ConverterObject* co =
      new ConverterObject(immortal, info.This(), info[0].As<String>());

  UErrorCode status = U_ZERO_ERROR;
  co->conv_ = ucnv_open(*name, &status);
  if (U_FAILURE(status)) {
    delete co;
    std::string err_str = SPrintF("ICU open failed(%s)", u_errorName(status));
    ThrowException(isolate, err_str.c_str(), ExceptionType::kTypeError);
    return;
  }

  UConverterToUCallback callback;
  if (fatal) {
    callback = UCNV_TO_U_CALLBACK_STOP;
  } else {
    callback = UCNV_TO_U_CALLBACK_SUBSTITUTE;
  }
  status = U_ZERO_ERROR;
  ucnv_setToUCallBack(co->conv_, callback, nullptr, nullptr, nullptr, &status);
  CHECK(U_SUCCESS(status));

  info.GetReturnValue().Set(info.This());
}

AWORKER_METHOD(ConverterObject::Decode) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  EscapableHandleScope scope(isolate);

  CHECK_EQ(info.Length(), 2);
  ConverterObject* co;
  ASSIGN_OR_RETURN_UNWRAP(&co, info.This());

  bool stream = info[1].As<Int32>()->Value() & CONVERTER_FLAGS_STREAM;

  Local<ArrayBufferView> input = info[0].As<ArrayBufferView>();
  std::shared_ptr<v8::BackingStore> input_bs =
      input->Buffer()->GetBackingStore();

  size_t length = input->ByteLength();

  const char* input_data =
      static_cast<char*>(input_bs->Data()) + input->ByteOffset();
  CHECK(input_data);

  size_t limit = co->min_char_size() * length;

  ResizableBuffer buf(limit * sizeof(UChar));
  UChar* buf_begin = static_cast<UChar*>(buf.buffer());
  UChar* buf_dest = static_cast<UChar*>(buf.buffer());

  UErrorCode status = U_ZERO_ERROR;
  ucnv_toUnicode(co->conv_,
                 &buf_dest,
                 buf_dest + limit,
                 &input_data,
                 input_data + length,
                 nullptr,
                 !stream,
                 &status);
  // Reset stateful conversion
  if (!stream) {
    ucnv_resetToUnicode(co->conv_);
  }

  if (U_FAILURE(status)) {
    std::string err_str =
        SPrintF("ucnv_toUnicode failed(%s)", u_errorName(status));
    ThrowException(isolate, err_str.c_str(), ExceptionType::kTypeError);
    return;
  }

  size_t string_length = buf_dest - buf_begin;
  if (!co->ignore_bom() && buf_begin[0] == 0xFEFF) {  // has initial bom head
    buf_begin += 1;
    string_length -= 1;
  }

  Local<String> output =
      String::NewFromTwoByte(isolate,
                             reinterpret_cast<const uint16_t*>(buf_begin),
                             NewStringType::kNormal,
                             string_length)
          .ToLocalChecked();
  info.GetReturnValue().Set(scope.Escape(output));
}

AWORKER_METHOD(EncodeIntoUtf8) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  EscapableHandleScope scope(isolate);

  Local<String> str = info[0].As<String>();
  Local<Uint8Array> dest = info[1].As<Uint8Array>();
  Local<ArrayBuffer> buf = dest->Buffer();

  char* write_result =
      static_cast<char*>(buf->GetBackingStore()->Data()) + dest->ByteOffset();
  size_t dest_length = dest->ByteLength();

  // results = [ read, written ]
  Local<Uint32Array> result_arr = info[2].As<Uint32Array>();
  uint32_t* results = reinterpret_cast<uint32_t*>(
      static_cast<char*>(result_arr->Buffer()->GetBackingStore()->Data()) +
      result_arr->ByteOffset());

  int nchars;
  int written = str->WriteUtf8(
      isolate,
      write_result,
      dest_length,
      &nchars,
      String::NO_NULL_TERMINATION | String::REPLACE_INVALID_UTF8);
  results[0] = nchars;
  results[1] = written;
}

AWORKER_METHOD(EncodeUtf8) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  EscapableHandleScope scope(isolate);

  Local<String> str = info[0].As<String>();
  size_t length = str->Utf8Length(isolate);
  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, length);

  char* buf = static_cast<char*>(ab->GetBackingStore()->Data());
  str->WriteUtf8(isolate,
                 buf,
                 -1,
                 nullptr,
                 String::NO_NULL_TERMINATION | String::REPLACE_INVALID_UTF8);

  Local<Uint8Array> buffer = Uint8Array::New(ab, 0, length);
  info.GetReturnValue().Set(scope.Escape(buffer));
}

AWORKER_BINDING(Init) {
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  Local<FunctionTemplate> tpl =
      FunctionTemplate::New(isolate, ConverterObject::New);
  tpl->InstanceTemplate()->SetInternalFieldCount(
      ConverterObject::kInternalFieldCount);

  Local<String> name = OneByteString(isolate, "Converter");
  tpl->SetClassName(name);

  Local<ObjectTemplate> prototype_template = tpl->PrototypeTemplate();
  immortal->SetFunctionProperty(
      prototype_template, "decode", ConverterObject::Decode);

  exports->Set(context, name, tpl->GetFunction(context).ToLocalChecked())
      .Check();

  // encoder
  immortal->SetFunctionProperty(exports, "encodeUtf8", EncodeUtf8);
  immortal->SetFunctionProperty(exports, "encodeIntoUtf8", EncodeIntoUtf8);

#define V(val)                                                                 \
  exports                                                                      \
      ->Set(context, OneByteString(isolate, #val), Int32::New(isolate, val))   \
      .Check();
  V(CONVERTER_FLAGS_NONE)
  V(CONVERTER_FLAGS_FATAL)
  V(CONVERTER_FLAGS_IGNORE_BOM)
  V(CONVERTER_FLAGS_STREAM)
#undef V
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  registry->Register(ConverterObject::New);
  registry->Register(ConverterObject::Decode);
  registry->Register(EncodeUtf8);
  registry->Register(EncodeIntoUtf8);
}

}  // namespace i18n
}  // namespace aworker

AWORKER_BINDING_REGISTER(i18n, aworker::i18n::Init, aworker::i18n::Init)
