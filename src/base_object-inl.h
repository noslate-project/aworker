#pragma once

#include "base_object.h"
#include "immortal.h"
#include "util.h"

#if (__GNUC__ >= 8) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
#include "v8.h"
#if (__GNUC__ >= 8) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace aworker {

BaseObject::BaseObject(Immortal* immortal, v8::Local<v8::Object> object)
    : persistent_(immortal->isolate(), object), immortal_(immortal) {
  CHECK_EQ(object.IsEmpty(), false);
  CHECK_GT(object->InternalFieldCount(), 0);
  object->SetAlignedPointerInInternalField(BaseObject::kSlot,
                                           static_cast<void*>(this));
  immortal->tracked_base_objects.insert(this);
}

BaseObject::~BaseObject() {
  immortal_->tracked_base_objects.erase(this);
  if (persistent_.IsEmpty()) {
    // This most likely happened because the weak callback below cleared it.
    return;
  }

  {
    v8::HandleScope handle_scope(immortal()->isolate());
    object()->SetAlignedPointerInInternalField(BaseObject::kSlot, nullptr);
  }
}

BaseObject* BaseObject::FromJSObject(v8::Local<v8::Value> value) {
  v8::Local<v8::Object> obj = value.As<v8::Object>();
  DCHECK_GE(obj->InternalFieldCount(), BaseObject::kSlot);
  return static_cast<BaseObject*>(
      obj->GetAlignedPointerFromInternalField(BaseObject::kSlot));
}

template <typename T>
T* BaseObject::FromJSObject(v8::Local<v8::Value> object) {
  return static_cast<T*>(FromJSObject(object));
}

v8::Local<v8::FunctionTemplate> BaseObject::GetConstructorTemplate(
    Immortal* immortal) {
  v8::Local<v8::FunctionTemplate> tpl = immortal->base_object_ctor_template();
  if (tpl.IsEmpty()) {
    tpl = v8::FunctionTemplate::New(immortal->isolate(), nullptr);
    tpl->SetClassName(OneByteString(immortal->isolate(), "BaseObject"));
    immortal->set_base_object_ctor_template(tpl);
  }
  return tpl;
}

template <int Field>
void BaseObject::InternalFieldGet(
    v8::Local<v8::String> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(info.This()->GetInternalField(Field));
}

template <int Field, bool (v8::Value::*typecheck)() const>
void BaseObject::InternalFieldSet(v8::Local<v8::String> property,
                                  v8::Local<v8::Value> value,
                                  const v8::PropertyCallbackInfo<void>& info) {
  // This could be e.g. value->IsFunction().
  CHECK(((*value)->*typecheck)());
  info.This()->SetInternalField(Field, value);
}

v8::Global<v8::Object>& BaseObject::persistent() {
  return persistent_;
}

v8::Local<v8::Object> BaseObject::object() const {
  return PersistentToLocal::Default(immortal()->isolate(), persistent_);
}

Immortal* BaseObject::immortal() const {
  return immortal_;
}

v8::Isolate* BaseObject::isolate() const {
  return immortal_->isolate();
}

void BaseObject::MakeWeak() {
  persistent_.SetWeak(
      this,
      [](const v8::WeakCallbackInfo<BaseObject>& data) {
        BaseObject* obj = data.GetParameter();
        // We have to reset the handle in weak callback.
        obj->persistent_.Reset();
        obj->OnGCCollect();
      },
      v8::WeakCallbackType::kParameter);
}

void BaseObject::ClearWeak() {
  persistent_.ClearWeak();
}

void BaseObject::OnGCCollect() {
  delete this;
}

}  // namespace aworker
