#ifndef SRC_DIAG_REPORT_H_
#define SRC_DIAG_REPORT_H_

#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

#include <sys/time.h>
#include <iomanip>
#include <sstream>
#include <string>

#include "immortal.h"
#include "v8.h"
#include "watchdog.h"

namespace aworker {
namespace report {

#ifdef _WIN32
typedef SYSTEMTIME TIME_TYPE;
#else  // UNIX, OSX
typedef struct tm TIME_TYPE;
#endif

class DiagnosticFilename {
 public:
  static void LocalTime(TIME_TYPE* tm_struct);

  inline DiagnosticFilename(const char* prefix, const char* ext);

  inline const char* operator*() const;

 private:
  static std::string MakeFilename(const char* prefix, const char* ext);

  std::string filename_;
};

inline DiagnosticFilename::DiagnosticFilename(const char* prefix,
                                              const char* ext)
    : filename_(MakeFilename(prefix, ext)) {}

inline const char* DiagnosticFilename::operator*() const {
  return filename_.c_str();
}

// Function declarations - functions in src/diag_report.cc
std::string TriggerDiagReport(v8::Isolate* isolate,
                              Immortal* immortal,
                              const char* message,
                              const char* trigger,
                              v8::Local<v8::Value> error);
void GetDiagReport(v8::Isolate* isolate,
                   Immortal* immortal,
                   const char* message,
                   const char* trigger,
                   v8::Local<v8::Value> error,
                   std::ostream& out);

void WalkHandle(uv_handle_t* h, void* arg);

template <typename T>
std::string ValueToHexString(T value) {
  std::stringstream hex;

  hex << "0x" << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex
      << value;
  return hex.str();
}

class ReportWatchdog : public SignalWatchdog {
 public:
  explicit ReportWatchdog(Immortal* immortal);

 protected:
  void OnSignal() override;

 private:
  Immortal* immortal_;
};

}  // namespace report
}  // namespace aworker

#endif  // SRC_DIAG_REPORT_H_
