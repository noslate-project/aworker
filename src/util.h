#pragma once

#include <limits.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/limits.h>
#endif
#include <algorithm>
#include <string>

#include "binding/core/text-inl.h"
#include "v8.h"

namespace aworker {

// Maybe remove kPathSeparator when cpp17 is ready
#ifdef _WIN32
constexpr char kPathSeparator = '\\';
/* MAX_PATH is in characters, not bytes. Make sure we have enough headroom. */
#define PATH_MAX_BYTES (MAX_PATH * 4)
#else
constexpr char kPathSeparator = '/';
#define PATH_MAX_BYTES (PATH_MAX)
#endif

struct AssertionInfo {
  const char* file_line;  // filename:line
  const char* message;
  const char* function;
};
[[noreturn]] void Assert(const AssertionInfo& info);
[[noreturn]] void Abort();

#define ABORT() aworker::Abort()

#define ERROR_AND_ABORT(expr)                                                  \
  do {                                                                         \
    /* Make sure that this struct does not end up in inline code, but      */  \
    /* rather in a read-only data section when modifying this code.        */  \
    static const aworker::AssertionInfo args = {                               \
        __FILE__ ":" STRINGIFY(__LINE__), #expr, PRETTY_FUNCTION_NAME};        \
    aworker::Assert(args);                                                     \
  } while (0)

#define LIKELY(expr) __builtin_expect(!!(expr), 1)
#define UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#define PRETTY_FUNCTION_NAME __PRETTY_FUNCTION__

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (UNLIKELY(!(expr))) {                                                   \
      ERROR_AND_ABORT(expr);                                                   \
    }                                                                          \
  } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_GE(a, b) CHECK((a) >= (b))
#define CHECK_GT(a, b) CHECK((a) > (b))
#define CHECK_LE(a, b) CHECK((a) <= (b))
#define CHECK_LT(a, b) CHECK((a) < (b))
#define CHECK_NE(a, b) CHECK((a) != (b))
#define CHECK_NULL(val) CHECK((val) == nullptr)
#define CHECK_NOT_NULL(val) CHECK((val) != nullptr)
#define CHECK_IMPLIES(a, b) CHECK(!(a) || (b))

#ifdef DEBUG
#define DCHECK(expr) CHECK(expr)
#define DCHECK_EQ(a, b) CHECK((a) == (b))
#define DCHECK_GE(a, b) CHECK((a) >= (b))
#define DCHECK_GT(a, b) CHECK((a) > (b))
#define DCHECK_LE(a, b) CHECK((a) <= (b))
#define DCHECK_LT(a, b) CHECK((a) < (b))
#define DCHECK_NE(a, b) CHECK((a) != (b))
#define DCHECK_NULL(val) CHECK((val) == nullptr)
#define DCHECK_NOT_NULL(val) CHECK((val) != nullptr)
#define DCHECK_IMPLIES(a, b) CHECK(!(a) || (b))
#else
#define DCHECK(expr)
#define DCHECK_EQ(a, b)
#define DCHECK_GE(a, b)
#define DCHECK_GT(a, b)
#define DCHECK_LE(a, b)
#define DCHECK_LT(a, b)
#define DCHECK_NE(a, b)
#define DCHECK_NULL(val)
#define DCHECK_NOT_NULL(val)
#define DCHECK_IMPLIES(a, b)
#endif

#define UNREACHABLE(...)                                                       \
  ERROR_AND_ABORT("Unreachable code reached" __VA_OPT__(": ") __VA_ARGS__)

#ifdef __GNUC__
#define MUST_USE_RESULT __attribute__((warn_unused_result))
#else
#define MUST_USE_RESULT
#endif

#define AWORKER_DISALLOW_ASSIGN(CLASS) void operator=(const CLASS&) = delete;
#define AWORKER_DISALLOW_COPY(CLASS) CLASS(const CLASS&) = delete;

#define AWORKER_DISALLOW_ASSIGN_COPY(CLASS)                                    \
  AWORKER_DISALLOW_ASSIGN(CLASS)                                               \
  AWORKER_DISALLOW_COPY(CLASS)

#define AWORKER_STATIC_ONLY(Type)                                              \
  Type() = delete;                                                             \
  Type(const Type&) = delete;                                                  \
  Type& operator=(const Type&) = delete;                                       \
  void* operator new(size_t) = delete;

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define AWORKER_SANITIZER_ENABLED 1
#endif
#elif defined(__SANITIZE_ADDRESS__)
#define AWORKER_SANITIZER_ENABLED 1
#endif

template <typename T>
inline void USE(T&&) {}

template <typename T>
inline T MultiplyWithOverflowCheck(T a, T b);

template <typename Fn>
struct OnScopeLeaveImpl {
  Fn fn_;
  bool active_;

  explicit OnScopeLeaveImpl(Fn&& fn) : fn_(std::move(fn)), active_(true) {}
  ~OnScopeLeaveImpl() {
    if (active_) fn_();
  }

  OnScopeLeaveImpl(const OnScopeLeaveImpl& other) = delete;
  OnScopeLeaveImpl& operator=(const OnScopeLeaveImpl& other) = delete;
  OnScopeLeaveImpl(OnScopeLeaveImpl&& other)
      : fn_(std::move(other.fn_)), active_(other.active_) {
    other.active_ = false;
  }
  OnScopeLeaveImpl& operator=(OnScopeLeaveImpl&& other) {
    if (this == &other) return *this;
    this->~OnScopeLeave();
    new (this) OnScopeLeaveImpl(std::move(other));
    return *this;
  }
};

// Run a function when exiting the current scope. Used like this:
// auto on_scope_leave = OnScopeLeave([&] {
//   // ... run some code ...
// });
template <typename Fn>
inline MUST_USE_RESULT OnScopeLeaveImpl<Fn> OnScopeLeave(Fn&& fn) {
  return OnScopeLeaveImpl<Fn>{std::move(fn)};
}

template <typename T, size_t N>
constexpr size_t arraysize(const T (&)[N]) {
  return N;
}

typedef struct env_item_s {
  char* name;
  char* value;
} env_item_t;
int os_environ(env_item_t** envitems, int* count);
void os_free_environ(env_item_t* envitems, int count);

int ReadFileSync(std::string* result, const char* path);

double GetCurrentTimeInMicroseconds();

inline std::string cwd() {
  char _cwd[PATH_MAX];
  CHECK_NOT_NULL(getcwd(_cwd, PATH_MAX));
  return _cwd;
}

class SlicedArguments : public std::vector<v8::Local<v8::Value>> {
 public:
  inline explicit SlicedArguments(
      const v8::FunctionCallbackInfo<v8::Value>& args, size_t start = 0);
};

// Convert a v8::PersistentBase, e.g. v8::Global, to a Local, with an extra
// optimization for strong persistent handles.
class PersistentToLocal {
 public:
  // If persistent.IsWeak() == false, then do not call persistent.Reset()
  // while the returned Local<T> is still in scope, it will destroy the
  // reference to the object.
  template <class TypeName>
  static inline v8::Local<TypeName> Default(
      v8::Isolate* isolate, const v8::PersistentBase<TypeName>& persistent) {
    if (persistent.IsWeak()) {
      return PersistentToLocal::Weak(isolate, persistent);
    } else {
      return PersistentToLocal::Strong(persistent);
    }
  }

  // Unchecked conversion from a non-weak Persistent<T> to Local<T>,
  // use with care!
  //
  // Do not call persistent.Reset() while the returned Local<T> is still in
  // scope, it will destroy the reference to the object.
  template <class TypeName>
  static inline v8::Local<TypeName> Strong(
      const v8::PersistentBase<TypeName>& persistent) {
    return *reinterpret_cast<v8::Local<TypeName>*>(
        const_cast<v8::PersistentBase<TypeName>*>(&persistent));
  }

  template <class TypeName>
  static inline v8::Local<TypeName> Weak(
      v8::Isolate* isolate, const v8::PersistentBase<TypeName>& persistent) {
    return v8::Local<TypeName>::New(isolate, persistent);
  }
};

// tolower() is locale-sensitive.  Use ToLower() instead.
inline char ToLower(char c);
inline std::string ToLower(const std::string& in);

// toupper() is locale-sensitive.  Use ToUpper() instead.
inline char ToUpper(char c);
inline std::string ToUpper(const std::string& in);

template <typename T, void (*function)(T*)>
struct FunctionDeleter {
  void operator()(T* pointer) const { function(pointer); }
  typedef std::unique_ptr<T, FunctionDeleter> Pointer;
};

template <typename T, void (*function)(T*)>
using DeleteFnPtr = typename FunctionDeleter<T, function>::Pointer;

template <typename Inner, typename Outer>
constexpr uintptr_t OffsetOf(Inner Outer::*field) {
  return reinterpret_cast<uintptr_t>(&(static_cast<Outer*>(nullptr)->*field));
}

// The helper is for doing safe downcasts from base types to derived types.
template <typename Inner, typename Outer>
class ContainerOfHelper {
 public:
  inline ContainerOfHelper(Inner Outer::*field, Inner* pointer)
      : pointer_(reinterpret_cast<Outer*>(reinterpret_cast<uintptr_t>(pointer) -
                                          OffsetOf(field))) {}

  template <typename TypeName>
  inline operator TypeName*() const {
    return static_cast<TypeName*>(pointer_);
  }

 private:
  Outer* const pointer_;
};

template <typename Inner, typename Outer>
constexpr ContainerOfHelper<Inner, Outer> ContainerOf(Inner Outer::*field,
                                                      Inner* pointer) {
  return ContainerOfHelper<Inner, Outer>(field, pointer);
}

}  // namespace aworker

#include "util-inl.h"
