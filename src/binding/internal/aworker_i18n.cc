#include <iostream>

#include "aworker_platform.h"
#include "error_handling.h"
#include "immortal.h"

#include "aworker_i18n.h"

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

size_t ConverterObject::max_char_size() const {
  return ucnv_getMaxCharSize(conv_);
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
    ThrowException(isolate, "icu open failed.");
    return;
  }

  if (fatal) {
    status = U_ZERO_ERROR;
    ucnv_setToUCallBack(
        co->conv_, UCNV_TO_U_CALLBACK_STOP, nullptr, nullptr, nullptr, &status);
  }

  info.GetReturnValue().Set(info.This());
}

AWORKER_METHOD(ConverterObject::Decode) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  Local<Context> context = immortal->context();
  EscapableHandleScope scope(isolate);

  CHECK_GE(info.Length(), 1);
  ConverterObject* co;
  ASSIGN_OR_RETURN_UNWRAP(&co, info.This());

  Local<ArrayBufferView> input = info[0].As<ArrayBufferView>();

  // TODO(zl131478): is unused variable flags useful?
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
  int flags = info[1]->Uint32Value(context).ToChecked();  // NOLINT
#pragma GCC diagnostic pop

  size_t length = input->ByteLength();
  size_t limit = co->min_char_size() * length;

  Local<ArrayBuffer> result =
      ArrayBuffer::New(isolate, (limit * sizeof(UChar)));
  Local<Uint8Array> resultview =
      Uint8Array::New(result, 0, (limit * sizeof(UChar)));

  const char* source =
      static_cast<char*>(input->Buffer()->GetBackingStore()->Data()) +
      input->ByteOffset();
  CHECK(source);

  size_t source_length = input->ByteLength();

  UErrorCode status = U_ZERO_ERROR;
  UChar* target =
      static_cast<UChar*>(resultview->Buffer()->GetBackingStore()->Data()) +
      resultview->ByteOffset();
  UChar* save = target;
  CHECK(save);

  ucnv_toUnicode(co->conv_,
                 &target,
                 target + limit,
                 &source,
                 source + source_length,
                 nullptr,
                 0,
                 &status);

  if (!U_SUCCESS(status)) {
    info.GetReturnValue().Set(status);
    return;
  }

  UChar* string_start = save;
  size_t string_length = target - save;

  if (string_length <= 0) {
    info.GetReturnValue().Set(status);
  }

  if (save[0] == 0xFEFF) {  // has initial bom head
    co->set_bom_seen(true);
    string_start += 1;
    string_length -= 1;
  }

  length = target - save;
  // std::cout << "====>>" <<  length << "," << target << "," << save <<
  // std::endl << std::endl;
  Local<String> output =
      String::NewFromTwoByte(isolate,
                             reinterpret_cast<const uint16_t*>(string_start),
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

  MaybeLocal<String> maybe_name = String::NewFromUtf8(isolate, "converter");
  CHECK(!maybe_name.IsEmpty());
  Local<String> name = maybe_name.ToLocalChecked();
  tpl->SetClassName(name);

  Local<ObjectTemplate> prototype_template = tpl->PrototypeTemplate();

  immortal->SetFunctionProperty(
      prototype_template, "decode", ConverterObject::Decode);

  exports->Set(context, name, tpl->GetFunction(context).ToLocalChecked())
      .Check();

  // encoder
  immortal->SetFunctionProperty(exports, "encodeUtf8", EncodeUtf8);
  immortal->SetFunctionProperty(exports, "encodeIntoUtf8", EncodeIntoUtf8);
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
