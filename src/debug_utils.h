#ifndef SRC_DEBUG_UTILS_H_
#define SRC_DEBUG_UTILS_H_
#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "util.h"
#include "uv.h"

// Use FORCE_INLINE on functions that have a debug-category-enabled check first
// and then ideally only a single function call following it, to maintain
// performance for the common case (no debugging used).
#ifdef __GNUC__
#define FORCE_INLINE __attribute__((always_inline))
#define COLD_NOINLINE __attribute__((cold, noinline))
#else
#define FORCE_INLINE
#define COLD_NOINLINE
#endif

namespace aworker {
#define DEBUG_CATEGORY_NAMES(V)                                                \
  V(AGENT_CHANNEL)                                                             \
  V(MACRO_TASK_QUEUE)                                                          \
  V(MKSNAPSHOT)                                                                \
  V(NATIVE_MODULE)                                                             \
  V(OVERRIDE)                                                                  \
  V(PLATFORM)                                                                  \
  V(PERFORMANCE)                                                               \
  V(TRACING)                                                                   \
  V(ZLIB)

enum class DebugCategory {
#define V(name) name,
  DEBUG_CATEGORY_NAMES(V)
#undef V
      CATEGORY_COUNT
};

class EnabledDebugList {
 public:
  bool enabled(DebugCategory category) const {
    DCHECK_GE(static_cast<int>(category), 0);
    DCHECK_LT(static_cast<int>(category),
              static_cast<int>(DebugCategory::CATEGORY_COUNT));
    return enabled_[static_cast<int>(category)];
  }

  // Uses AWORKER_DEBUG to initialize the categories. The system
  // environment variables are used.
  void Parse();

 private:
  // Set all categories matching cats to the value of enabled.
  void Parse(const std::string& cats, bool enabled);
  void set_enabled(DebugCategory category, bool enabled) {
    DCHECK_GE(static_cast<int>(category), 0);
    DCHECK_LT(static_cast<int>(category),
              static_cast<int>(DebugCategory::CATEGORY_COUNT));
    enabled_[static_cast<int>(category)] = true;
  }

  bool enabled_[static_cast<int>(DebugCategory::CATEGORY_COUNT)] = {false};
};

template <typename... Args>
inline void FORCE_INLINE Debug(EnabledDebugList* list,
                               DebugCategory cat,
                               const char* format,
                               Args&&... args);

inline void FORCE_INLINE Debug(EnabledDebugList* list,
                               DebugCategory cat,
                               const char* message);

template <typename T>
inline std::string ToString(const T& value);

// C++-style variant of sprintf()/fprintf() that:
// - Returns an std::string
// - Handles \0 bytes correctly
// - Supports %p and %s. %d, %i and %u are aliases for %s.
// - Accepts any class that has a ToString() method for stringification.
template <typename... Args>
inline std::string SPrintF(const char* format, Args&&... args);
template <typename... Args>
inline void FPrintF(FILE* file, const char* format, Args&&... args);
void FWrite(FILE* file, const std::string& str);

// Debug helper for inspecting the currently running `node` executable.
class NativeSymbolDebuggingContext {
 public:
  static std::unique_ptr<NativeSymbolDebuggingContext> New();

  class SymbolInfo {
   public:
    std::string name;
    std::string filename;
    size_t line = 0;
    size_t dis = 0;

    std::string Display() const;
  };

  NativeSymbolDebuggingContext() = default;
  virtual ~NativeSymbolDebuggingContext() = default;

  virtual SymbolInfo LookupSymbol(void* address) { return {}; }
  virtual bool IsMapped(void* address) { return false; }
  virtual int GetStackTrace(void** frames, int count) { return 0; }

  NativeSymbolDebuggingContext(const NativeSymbolDebuggingContext&) = delete;
  NativeSymbolDebuggingContext(NativeSymbolDebuggingContext&&) = delete;
  NativeSymbolDebuggingContext operator=(NativeSymbolDebuggingContext&) =
      delete;
  NativeSymbolDebuggingContext operator=(NativeSymbolDebuggingContext&&) =
      delete;
  static std::vector<std::string> GetLoadedLibraries();
};

void CheckedUvLoopClose(uv_loop_t* loop);
void PrintUvHandleInformation(uv_loop_t* loop, FILE* stream);

const char* VmStateToString(v8::StateTag stateTag);
void PrintJavaScriptStack(v8::Isolate* isolate, const char* message);

namespace per_process {
extern EnabledDebugList enabled_debug_list;

template <typename... Args>
inline void FORCE_INLINE Debug(DebugCategory cat,
                               const char* format,
                               Args&&... args);

inline void FORCE_INLINE Debug(DebugCategory cat, const char* message);
inline void FORCE_INLINE DebugPrintCurrentTime(const char* name);
}  // namespace per_process
}  // namespace aworker

#include "debug_utils-inl.h"

#endif  // SRC_DEBUG_UTILS_H_
