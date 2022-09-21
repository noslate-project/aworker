#pragma once

#include <type_traits>  // std::remove_reference
#include "v8.h"

namespace aworker {

class Immortal;

// This struct provides a way to store a bunch of information that is helpful
// when unwrapping v8 objects. Each v8 bindings class has exactly one static
// WrapperTypeInfo member, so comparing pointers is a safe way to determine if
// types match.
struct WrapperTypeInfo final {
  /** Disallow new */
 public:
  void* operator new(size_t, void* location) { return location; }

 private:
  void* operator new(size_t) = delete;

 public:
  const char* interface_name;

  // TODO(chengzhong.wcz): function template allocator with known setups.
};

// Defines |GetWrapperTypeInfo| virtual method which returns the WrapperTypeInfo
// of the instance. Also declares a static member of type WrapperTypeInfo, of
// which the definition is given by the IDL code generator.
//
// All the derived classes of BaseObject, must write this macro in the class
// definition.
#define DEFINE_WRAPPERTYPEINFO()                                               \
 public:                                                                       \
  const WrapperTypeInfo* GetWrapperTypeInfo() const override {                 \
    return &wrapper_type_info_;                                                \
  }                                                                            \
  static const WrapperTypeInfo* GetStaticWrapperTypeInfo() {                   \
    return &wrapper_type_info_;                                                \
  }                                                                            \
                                                                               \
 private:                                                                      \
  static const WrapperTypeInfo wrapper_type_info_

#define DECLARE_INTERNAL_FIELD(name, type, id)                                 \
  v8::Local<v8::Object> name() const;                                          \
  type* name##Ptr() const;                                                     \
  void Set##name##Ptr(type* ptr);

#define DEFINE_INTERNAL_FIELD(Type, name, type, id)                            \
  v8::Local<v8::Object> Type::name() const {                                   \
    v8::Local<v8::Value> value =                                               \
        object()->GetInternalField(id).As<v8::Object>();                       \
    return value.As<v8::Object>();                                             \
  }                                                                            \
  type* Type::name##Ptr() const {                                              \
    v8::Local<v8::Object> value = name();                                      \
    type* ptr = static_cast<type*>(BaseObject::FromJSObject(value));           \
    return ptr;                                                                \
  }                                                                            \
  void Type::Set##name##Ptr(type* ptr) {                                       \
    object()->SetInternalField(id, ptr->object());                             \
  }

class BaseObject {
 public:
  enum InternalFields { kSlot, kInternalFieldCount };

  // Associates this object with `object`. It uses the 0th internal field for
  // that, and in particular aborts if there is no such field.
  inline BaseObject(Immortal* immortal, v8::Local<v8::Object> object);
  virtual inline ~BaseObject();

  BaseObject() = delete;

  virtual const WrapperTypeInfo* GetWrapperTypeInfo() const = 0;

  // Get a BaseObject* pointer, or subclass pointer, for the JS object that
  // was also passed to the `BaseObject()` constructor initially.
  // This may return `nullptr` if the C++ object has not been constructed yet,
  // e.g. when the JS object used `MakeLazilyInitializedJSTemplate`.
  static inline BaseObject* FromJSObject(v8::Local<v8::Value> object);
  template <typename T>
  static inline T* FromJSObject(v8::Local<v8::Value> object);

  static inline v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Immortal* env);

  // Setter/Getter pair for internal fields that can be passed to SetAccessor.
  template <int Field>
  static void InternalFieldGet(v8::Local<v8::String> property,
                               const v8::PropertyCallbackInfo<v8::Value>& info);
  template <int Field, bool (v8::Value::*typecheck)() const>
  static void InternalFieldSet(v8::Local<v8::String> property,
                               v8::Local<v8::Value> value,
                               const v8::PropertyCallbackInfo<void>& info);

  // Returns the wrapped object. Returns an empty handle when
  // persistent.IsEmpty() is true.
  inline v8::Local<v8::Object> object() const;
  inline v8::Global<v8::Object>& persistent();

  inline Immortal* immortal() const;
  inline v8::Isolate* isolate() const;

  // Make the `v8::Global` a weak reference and, `delete` this object once
  // the JS object has been garbage collected and there are no (strong)
  // BaseObjectPtr references to it.
  inline void MakeWeak();

  // Undo `MakeWeak()`, i.e. turn this into a strong reference that is a GC
  // root and will not be touched by the garbage collector.
  inline void ClearWeak();

  // Can be overriden to customize behaviors on GC.
  virtual inline void OnGCCollect();

  virtual bool is_snapshotable() const { return false; }

  // TODO(chengzhong.wcz): Transferable object.

 private:
  v8::Global<v8::Object> persistent_;
  Immortal* immortal_;
};

// Global alias for FromJSObject() to avoid churn.
template <typename T>
inline T* Unwrap(v8::Local<v8::Value> obj) {
  return BaseObject::FromJSObject<T>(obj);
}

// Use like:
// ```c++
// AsyncWrap* wrap;
// ASSIGN_OR_RETURN_UNWRAP(&wrap, info.This());
// ```
#define ASSIGN_OR_RETURN_UNWRAP(ptr, obj, ...)                                 \
  do {                                                                         \
    *ptr = static_cast<typename std::remove_reference<decltype(*ptr)>::type>(  \
        BaseObject::FromJSObject(obj));                                        \
    DCHECK_NE(*ptr, nullptr);                                                  \
    if (*ptr == nullptr) return __VA_ARGS__;                                   \
  } while (0)

}  // namespace aworker

#include "base_object-inl.h"
