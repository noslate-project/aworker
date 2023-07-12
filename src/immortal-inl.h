#ifndef SRC_IMMORTAL_INL_H_
#define SRC_IMMORTAL_INL_H_

#include "immortal.h"
#include "util.h"
#include "v8.h"

namespace aworker {

inline Immortal* Immortal::GetCurrent(v8::Isolate* isolate) {
  if (UNLIKELY(!isolate->InContext())) return nullptr;
  v8::HandleScope handle_scope(isolate);
  return GetCurrent(isolate->GetCurrentContext());
}

inline Immortal* Immortal::GetCurrent(v8::Local<v8::Context> context) {
  if (UNLIKELY(!ContextEmbedderTag::IsContextValid(context))) {
    return nullptr;
  }
  return static_cast<Immortal*>(context->GetAlignedPointerFromEmbedderData(
      ContextEmbedderIndex::kImmortal));
}

inline Immortal* Immortal::GetCurrent(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  return GetCurrent(info.GetIsolate()->GetCurrentContext());
}

template <typename T>
inline Immortal* Immortal::GetCurrent(const v8::PropertyCallbackInfo<T>& info) {
  return GetCurrent(info.GetIsolate()->GetCurrentContext());
}

inline v8::Local<v8::Object> Immortal::global_object() {
  return context()->Global();
}

#define V(type, name)                                                          \
  void Immortal::set_##name(type value) { this->name##_ = value; }             \
  type Immortal::name() { return this->name##_; }
IMMORTAL_NORMAL_PROPERTIES(V)
#undef V

#define V(type, name)                                                          \
  void Immortal::set_##name(v8::Local<type> value) {                           \
    name##_.Reset(isolate_, value);                                            \
  }                                                                            \
  v8::Local<type> Immortal::name() {                                           \
    return v8::Local<type>::New(isolate_, name##_);                            \
  }
IMMORTAL_GLOBAL_PROPERTIES(V)
#undef V

#define V(type, name)                                                          \
  v8::Local<type> Immortal::name() { return isolate_data_->name(); }
#define VS(name, _) V(v8::String, name##_string)
#define VP(name, _) V(v8::Private, name##_symbol)
PER_ISOLATE_STRING_PROPERTIES(VS);
PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP);
#undef VS
#undef VP
#undef V

#define V(type, name)                                                          \
  v8::Local<type> IsolateData::name() {                                        \
    DCHECK(!name##_.IsEmpty());                                                \
    return v8::Local<type>::New(isolate_, name##_);                            \
  }
#define VS(name, _) V(v8::String, name##_string)
#define VP(name, _) V(v8::Private, name##_symbol)
PER_ISOLATE_STRING_PROPERTIES(VS);
PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP);
#undef VS
#undef VP
#undef V

}  // namespace aworker

#endif  // SRC_IMMORTAL_INL_H_
