#ifndef SRC_ERROR_HANDLING_H_
#define SRC_ERROR_HANDLING_H_

#include "immortal.h"
#include "v8.h"

namespace aworker {

enum class CatchMode { kNormal, kFatal };
class TryCatchScope : public v8::TryCatch {
 public:
  explicit TryCatchScope(Immortal* immortal,
                         CatchMode mode = CatchMode::kNormal)
      : v8::TryCatch(immortal->isolate()), _immortal(immortal), _mode(mode) {}
  ~TryCatchScope();

  TryCatchScope(TryCatchScope&) = delete;
  TryCatchScope(TryCatchScope&&) = delete;
  TryCatchScope operator=(TryCatchScope&) = delete;
  TryCatchScope operator=(TryCatchScope&&) = delete;

 private:
  // Declaring operator new and delete as deleted is not spec compliant.
  // Therefore declare them private instead to disable dynamic alloc
  void* operator new(std::size_t count) noexcept { return nullptr; }
  void* operator new[](std::size_t count) noexcept { return nullptr; }
  void operator delete(void*, size_t) {}
  void operator delete[](void*, size_t) {}

  Immortal* _immortal;
  CatchMode _mode;
};

#define ERRORS_WITH_CODE(V)                                                    \
  V(ERR_STRING_TOO_LONG, TypeError)                                            \
  V(ERR_INSPECTOR_NOT_ACTIVE, TypeError)                                       \
  V(ERR_INVALID_ARGUMENT, TypeError)                                           \
  V(ERR_FUNCTION_EXPECTED, TypeError)

#define V(code, type)                                                          \
  inline v8::Local<v8::Value> code(v8::Isolate* isolate,                       \
                                   const char* message) {                      \
    v8::Local<v8::String> js_code = OneByteString(isolate, #code);             \
    v8::Local<v8::String> js_msg = OneByteString(isolate, message);            \
    v8::Local<v8::Object> e = v8::Exception::type(js_msg)                      \
                                  ->ToObject(isolate->GetCurrentContext())     \
                                  .ToLocalChecked();                           \
    e->Set(isolate->GetCurrentContext(),                                       \
           OneByteString(isolate, "code"),                                     \
           js_code)                                                            \
        .Check();                                                              \
    return e;                                                                  \
  }                                                                            \
  inline void THROW_##code(v8::Isolate* isolate, const char* message) {        \
    isolate->ThrowException(code(isolate, message));                           \
  }                                                                            \
  inline void THROW_##code(Immortal* immortal, const char* message) {          \
    THROW_##code(immortal->isolate(), message);                                \
  }
ERRORS_WITH_CODE(V)
#undef V

inline v8::Local<v8::Value> ERR_STRING_TOO_LONG(v8::Isolate* isolate) {
  char message[128];
  snprintf(message,
           sizeof(message),
           "Cannot create a string longer than 0x%x characters",
           v8::String::kMaxLength);
  return ERR_STRING_TOO_LONG(isolate, message);
}

enum class ExceptionType {
  kError,
  kRangeError,
  kReferenceError,
  kSyntaxError,
  kTypeError,
};

void ThrowException(v8::Isolate* isolate,
                    const char* message,
                    ExceptionType type = ExceptionType::kError);
v8::Local<v8::Value> GenException(v8::Isolate* isolate,
                                  const char* message,
                                  ExceptionType type = ExceptionType::kError);
void PrintCaughtException(v8::Local<v8::Context> context,
                          const v8::TryCatch& try_catch);
void PrintCaughtException(v8::Local<v8::Context> context,
                          v8::Local<v8::Value> exception);
void PrintStackTrace(v8::Isolate* isolate, v8::Local<v8::Message> message);

// Fatal handlers
[[noreturn]] void FatalException(v8::Isolate* isolate,
                                 v8::Local<v8::Value> exception,
                                 v8::Local<v8::Message> message);
[[noreturn]] void FatalError(const char* location, const char* message);

// Internal message functions.
namespace errors {
void EmitUncaughtException(v8::Isolate* isolate,
                           v8::Local<v8::Value> error,
                           v8::Local<v8::Message> message);
void EmitUncaughtException(v8::Isolate* isolate, const v8::TryCatch& try_catch);

// Per Isolate Handlers
void OnMessage(v8::Local<v8::Message> message, v8::Local<v8::Value> error);
void OnPromiseReject(v8::PromiseRejectMessage message);

}  // namespace errors
}  // namespace aworker

#endif  // SRC_ERROR_HANDLING_H_
