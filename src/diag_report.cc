#ifdef _WIN32
#include <Windows.h>
#else  // !_WIN32
#include <cxxabi.h>
#include <dlfcn.h>
#include <sys/resource.h>
#endif

#include <sys/utsname.h>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <cwctype>
#include <fstream>
#include <iostream>
#include <sstream>

#include "command_parser.h"
#include "debug_utils.h"

#include "aworker_version.h"
#include "diag_report.h"
#include "json_utils.h"
#include "metadata.h"
#include "util.h"

constexpr int DIAG_REPORT_VERSION = 2;
constexpr int NANOS_PER_SEC = 1000 * 1000 * 1000;
constexpr double SEC_PER_MICROS = 1e-6;

#ifdef MAXHOSTNAMELEN
#define MAXHOSTNAMESIZE (MAXHOSTNAMELEN + 1)
#else
/*
  Fallback for the maximum hostname size, including the null terminator. The
  Windows gethostname() documentation states that 256 bytes will always be
  large enough to hold the null-terminated hostname.
*/
#define MAXHOSTNAMESIZE 256
#endif

namespace aworker {
namespace report {
using aworker::JSONWriter;
using v8::Array;
using v8::Context;
using v8::HandleScope;
using v8::HeapSpaceStatistics;
using v8::HeapStatistics;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::RegisterState;
using v8::SampleInfo;
using v8::StackFrame;
using v8::StackTrace;
using v8::String;
using v8::TryCatch;
using v8::V8;
using v8::Value;

// Internal/static function declarations
static void WriteDiagReport(Isolate* isolate,
                            Immortal* immortal,
                            const char* message,
                            const char* trigger,
                            const std::string& filename,
                            std::ostream& out,
                            Local<Value> error,
                            bool compact);
static void PrintVersionInformation(JSONWriter* writer);
static void PrintJavaScriptErrorStack(JSONWriter* writer,
                                      Isolate* isolate,
                                      Local<Value> error,
                                      const char* trigger);
static void PrintJavaScriptErrorProperties(JSONWriter* writer,
                                           Isolate* isolate,
                                           Local<Value> error);
static void PrintNativeStack(JSONWriter* writer);
static void PrintGCStatistics(JSONWriter* writer, Isolate* isolate);
static void PrintUvLoopResources(JSONWriter* writer, Immortal* immortal);
static void PrintResourceUsage(JSONWriter* writer, Immortal* immortal);
static void PrintSystemInformation(JSONWriter* writer);
static void PrintLoadedLibraries(JSONWriter* writer);
static void PrintComponentVersions(JSONWriter* writer);

// External function to trigger a report, writing to file.
std::string TriggerDiagReport(Isolate* isolate,
                              Immortal* immortal,
                              const char* message,
                              const char* trigger,
                              Local<Value> error) {
  std::string filename = immortal->commandline_parser()->report_filename();

  // Determine the required report filename. In order of priority:
  //   1) supplied on API 2) configured on startup 3) default generated
  if (filename.empty()) {
    filename = *DiagnosticFilename("report", "json");
  }

  // Open the report file stream for writing. Supports stdout/err,
  // user-specified or (default) generated name
  std::ofstream outfile;
  std::ostream* outstream;
  if (filename == "stdout") {
    outstream = &std::cout;
  } else if (filename == "stderr") {
    outstream = &std::cerr;
  } else {
    std::string report_directory =
        immortal->commandline_parser()->report_directory();
    // Regular file. Append filename to directory path if one was specified
    if (report_directory.length() > 0) {
      std::string pathname = report_directory;
      pathname += aworker::kPathSeparator;
      pathname += filename;
      outfile.open(pathname, std::ios::out | std::ios::binary);
    } else {
      outfile.open(filename, std::ios::out | std::ios::binary);
    }
    // Check for errors on the file open
    if (!outfile.is_open()) {
      std::cerr << "\nFailed to open diagnostic report file: " << filename;

      if (report_directory.length() > 0)
        std::cerr << " directory: " << report_directory;

      std::cerr << " (errno: " << errno << ")" << std::endl;
      return "";
    }
    outstream = &outfile;
    std::cerr << "\nWriting diagnostic report to file: " << filename;
  }

  bool compact = immortal->commandline_parser()->report_compact();
  WriteDiagReport(isolate,
                  immortal,
                  message,
                  trigger,
                  filename,
                  *outstream,
                  error,
                  compact);

  // Do not close stdout/stderr, only close files we opened.
  if (outfile.is_open()) {
    outfile.close();
  }

  // Do not mix JSON and free-form text on stderr.
  if (filename != "stderr") {
    std::cerr << "\nDiagnostic report completed" << std::endl;
  }
  return filename;
}

// External function to trigger a report, writing to a supplied stream.
void GetDiagReport(Isolate* isolate,
                   Immortal* immortal,
                   const char* message,
                   const char* trigger,
                   Local<Value> error,
                   std::ostream& out) {
  WriteDiagReport(isolate, immortal, message, trigger, "", out, error, false);
}

// Internal function to coordinate and write the various
// sections of the report to the supplied stream
static void WriteDiagReport(Isolate* isolate,
                            Immortal* immortal,
                            const char* message,
                            const char* trigger,
                            const std::string& filename,
                            std::ostream& out,
                            Local<Value> error,
                            bool compact) {
  // Obtain the current time and the pid.
  TIME_TYPE tm_struct;
  DiagnosticFilename::LocalTime(&tm_struct);
  pid_t pid = getpid();

  // Save formatting for output stream.
  std::ios old_state(nullptr);
  old_state.copyfmt(out);

  // File stream opened OK, now start printing the report content:
  // the title and header information (event, filename, timestamp and pid)

  JSONWriter writer(out, compact);
  writer.json_start();
  writer.json_objectstart("header");
  writer.json_keyvalue("reportVersion", DIAG_REPORT_VERSION);
  writer.json_keyvalue("event", message);
  writer.json_keyvalue("trigger", trigger);
  if (!filename.empty())
    writer.json_keyvalue("filename", filename);
  else
    writer.json_keyvalue("filename", JSONWriter::Null{});

  // Report dump event and module load date/time stamps
  char timebuf[64];
#ifdef _WIN32
  snprintf(timebuf,
           sizeof(timebuf),
           "%4d-%02d-%02dT%02d:%02d:%02dZ",
           tm_struct.wYear,
           tm_struct.wMonth,
           tm_struct.wDay,
           tm_struct.wHour,
           tm_struct.wMinute,
           tm_struct.wSecond);
  writer.json_keyvalue("dumpEventTime", timebuf);
#else  // UNIX, OSX
  snprintf(timebuf,
           sizeof(timebuf),
           "%4d-%02d-%02dT%02d:%02d:%02dZ",
           tm_struct.tm_year + 1900,
           tm_struct.tm_mon + 1,
           tm_struct.tm_mday,
           tm_struct.tm_hour,
           tm_struct.tm_min,
           tm_struct.tm_sec);
  writer.json_keyvalue("dumpEventTime", timebuf);
#endif

  struct timeval time_val;
  if (gettimeofday(&time_val, nullptr) == 0) {
    writer.json_keyvalue(
        "dumpEventTimeStamp",
        std::to_string(time_val.tv_sec * 1000 + time_val.tv_usec / 1000));
  }

  // Report native process ID
  writer.json_keyvalue("processId", pid);
  {
    // Report the process cwd.
    char buf[PATH_MAX_BYTES];
    if (getcwd(buf, PATH_MAX_BYTES) != NULL) writer.json_keyvalue("cwd", buf);
  }

  // TODO(chengzhong.wcz): Report out the command line.

  // Report Aworker and OS version information
  PrintVersionInformation(&writer);
  writer.json_objectend();

  writer.json_objectstart("javascriptStack");
  // Report summary JavaScript error stack backtrace
  PrintJavaScriptErrorStack(&writer, isolate, error, trigger);

  // Report summary JavaScript error properties backtrace
  PrintJavaScriptErrorProperties(&writer, isolate, error);

  writer.json_objectend();  // the end of 'javascriptStack'

  // Report native stack backtrace
  PrintNativeStack(&writer);

  // Report V8 Heap and Garbage Collector information
  PrintGCStatistics(&writer, isolate);

  // Report libuv resource usage
  PrintUvLoopResources(&writer, immortal);

  // Report OS and current thread resource usage
  PrintResourceUsage(&writer, immortal);

  // Report operating system information
  PrintSystemInformation(&writer);

  writer.json_objectend();

  // Restore output stream formatting.
  out.copyfmt(old_state);
}

// Report Aworker version, OS version and machine information.
static void PrintVersionInformation(JSONWriter* writer) {
  std::ostringstream buf;
  writer->json_keyvalue("aworkerVersion", AWORKER_VERSION_STRING);

#ifndef _WIN32
  // Report compiler and runtime glibc versions where possible.
  const char* (*libc_version)();
  *(reinterpret_cast<void**>(&libc_version)) =
      dlsym(RTLD_DEFAULT, "gnu_get_libc_version");
  if (libc_version != nullptr)
    writer->json_keyvalue("glibcVersionRuntime", (*libc_version)());
#endif /* _WIN32 */

#ifdef __GLIBC__
  buf << __GLIBC__ << "." << __GLIBC_MINOR__;
  writer->json_keyvalue("glibcVersionCompiler", buf.str());
  buf.str("");
#endif

  // Report Process word size
  writer->json_keyvalue("wordSize", sizeof(void*) * 8);
  // Per process metadata
  writer->json_keyvalue("arch", AWORKER_ARCH);
  writer->json_keyvalue("platform", AWORKER_PLATFORM);

  // Report deps component versions
  PrintComponentVersions(writer);

  // Report operating system and machine information
  struct utsname os_info;

  if (uname(&os_info) == 0) {
    writer->json_keyvalue("osName", os_info.sysname);
    writer->json_keyvalue("osRelease", os_info.release);
    writer->json_keyvalue("osVersion", os_info.version);
    writer->json_keyvalue("osMachine", os_info.machine);
  }

  char host[MAXHOSTNAMESIZE];
  if (gethostname(host, MAXHOSTNAMESIZE) == 0)
    writer->json_keyvalue("host", host);
}

static void PrintJavaScriptErrorProperties(JSONWriter* writer,
                                           Isolate* isolate,
                                           Local<Value> error) {
  writer->json_objectstart("errorProperties");
  if (!error.IsEmpty() && error->IsObject()) {
    TryCatch try_catch(isolate);
    Local<Object> error_obj = error.As<Object>();
    Local<Context> context = isolate->GetCurrentContext();
    Local<Array> keys;
    if (!error_obj->GetOwnPropertyNames(context).ToLocal(&keys)) {
      return writer->json_objectend();  // the end of 'errorProperties'
    }
    uint32_t keys_length = keys->Length();
    for (uint32_t i = 0; i < keys_length; i++) {
      Local<Value> key;
      if (!keys->Get(context, i).ToLocal(&key) || !key->IsString()) {
        continue;
      }
      Local<Value> value;
      Local<String> value_string;
      if (!error_obj->Get(context, key).ToLocal(&value) ||
          !value->ToString(context).ToLocal(&value_string)) {
        continue;
      }
      String::Utf8Value k(isolate, key);
      if (!strcmp(*k, "stack") || !strcmp(*k, "message")) continue;
      String::Utf8Value v(isolate, value_string);
      writer->json_keyvalue(std::string(*k, k.length()),
                            std::string(*v, v.length()));
    }
  }
  writer->json_objectend();  // the end of 'errorProperties'
}

// Report the JavaScript stack.
static const size_t kMaxFramesCount = 255;
static void PrintJavaScriptErrorStackNoEval(JSONWriter* writer,
                                            Isolate* isolate,
                                            const char* trigger) {
  HandleScope scope(isolate);
  RegisterState state;
  SampleInfo info;

  // init state
  state.pc = nullptr;
  state.fp = &state;
  state.sp = &state;

  // instruction pointer
  void* samples[kMaxFramesCount];

  // get instruction pointer
  isolate->GetStackSample(state, samples, kMaxFramesCount, &info);
  if (info.frames_count == 0) {
    writer->json_keyvalue(
        "message",
        SPrintF("No stack, VmState: %s", VmStateToString(info.vm_state)));
    writer->json_arraystart("stack");
    writer->json_element("Unavailable.");
    writer->json_arrayend();
    return;
  }

  // get js stacks
  Local<StackTrace> stack = StackTrace::CurrentStackTrace(
      isolate, kMaxFramesCount, StackTrace::kDetailed);
  writer->json_keyvalue("message", trigger);
  writer->json_arraystart("stack");
  for (int i = 0; i < stack->GetFrameCount(); i++) {
    Local<StackFrame> frame = stack->GetFrame(isolate, i);

    Utf8Value function_name(isolate, frame->GetFunctionName());
    Utf8Value script_name(isolate, frame->GetScriptName());
    const int line_number = frame->GetLineNumber();
    const int column_number = frame->GetColumn();
    std::string line = SPrintF("at %s (%s:%d:%d)",
                               *function_name,
                               *script_name,
                               line_number,
                               column_number);

    writer->json_element(line);
  }
  writer->json_arrayend();
}

static void PrintJavaScriptErrorStack(JSONWriter* writer,
                                      Isolate* isolate,
                                      Local<Value> error,
                                      const char* trigger) {
  if ((!strcmp(trigger, "FatalError")) || (!strcmp(trigger, "Signal"))) {
    return PrintJavaScriptErrorStackNoEval(writer, isolate, trigger);
  }

  Local<Context> context = isolate->GetCurrentContext();
  Local<Value> stackstr;
  std::string ss = "";
  TryCatch try_catch(isolate);
  if (!error.IsEmpty() && error->IsObject()) {
    Local<Object> err_obj = error.As<Object>();
    if (err_obj->Get(context, OneByteString(isolate, "stack"))
            .ToLocal(&stackstr)) {
      String::Utf8Value sv(isolate, stackstr);
      ss = std::string(*sv, sv.length());
    }
  } else if (!error.IsEmpty() && error->ToString(context).ToLocal(&stackstr)) {
    String::Utf8Value sv(isolate, stackstr);
    ss = std::string(*sv, sv.length());
  }
  int line = ss.find('\n');
  if (line == -1) {
    writer->json_keyvalue("message", ss);
  } else {
    std::string l = ss.substr(0, line);
    writer->json_keyvalue("message", l);
    writer->json_arraystart("stack");
    ss = ss.substr(line + 1);
    line = ss.find('\n');
    while (line != -1) {
      l = ss.substr(0, line);
      l.erase(l.begin(), std::find_if(l.begin(), l.end(), [](int ch) {
                return !std::iswspace(ch);
              }));
      writer->json_element(l);
      ss = ss.substr(line + 1);
      line = ss.find('\n');
    }
    writer->json_arrayend();
  }
}

// Report a native stack backtrace
static void PrintNativeStack(JSONWriter* writer) {
  auto sym_ctx = NativeSymbolDebuggingContext::New();
  void* frames[256];
  const int size = sym_ctx->GetStackTrace(frames, arraysize(frames));
  writer->json_arraystart("nativeStack");
  int i;
  for (i = 1; i < size; i++) {
    void* frame = frames[i];
    writer->json_start();
    writer->json_keyvalue("pc",
                          ValueToHexString(reinterpret_cast<uintptr_t>(frame)));
    writer->json_keyvalue("symbol", sym_ctx->LookupSymbol(frame).Display());
    writer->json_end();
  }
  writer->json_arrayend();
}

// Report V8 JavaScript heap information.
// This uses the existing V8 HeapStatistics and HeapSpaceStatistics APIs.
// The isolate->GetGCStatistics(&heap_stats) internal V8 API could potentially
// provide some more useful information - the GC history and the handle counts
static void PrintGCStatistics(JSONWriter* writer, Isolate* isolate) {
  HeapStatistics v8_heap_stats;
  isolate->GetHeapStatistics(&v8_heap_stats);
  HeapSpaceStatistics v8_heap_space_stats;

  writer->json_objectstart("javascriptHeap");
  writer->json_keyvalue("totalMemory", v8_heap_stats.total_heap_size());
  writer->json_keyvalue("totalCommittedMemory",
                        v8_heap_stats.total_physical_size());
  writer->json_keyvalue("usedMemory", v8_heap_stats.used_heap_size());
  writer->json_keyvalue("availableMemory",
                        v8_heap_stats.total_available_size());
  writer->json_keyvalue("memoryLimit", v8_heap_stats.heap_size_limit());

  writer->json_objectstart("heapSpaces");
  // Loop through heap spaces
  for (size_t i = 0; i < isolate->NumberOfHeapSpaces(); i++) {
    isolate->GetHeapSpaceStatistics(&v8_heap_space_stats, i);
    writer->json_objectstart(v8_heap_space_stats.space_name());
    writer->json_keyvalue("memorySize", v8_heap_space_stats.space_size());
    writer->json_keyvalue("committedMemory",
                          v8_heap_space_stats.physical_space_size());
    writer->json_keyvalue("capacity",
                          v8_heap_space_stats.space_used_size() +
                              v8_heap_space_stats.space_available_size());
    writer->json_keyvalue("used", v8_heap_space_stats.space_used_size());
    writer->json_keyvalue("available",
                          v8_heap_space_stats.space_available_size());
    writer->json_objectend();
  }

  writer->json_objectend();
  writer->json_objectend();
}

static void PrintUvLoopResources(JSONWriter* writer, Immortal* immortal) {
  writer->json_arraystart("libuv");
  if (immortal != nullptr) {
    uv_walk(immortal->event_loop(), WalkHandle, static_cast<void*>(writer));

    writer->json_start();
    writer->json_keyvalue("type", "loop");
    writer->json_keyvalue(
        "is_active", static_cast<bool>(uv_loop_alive(immortal->event_loop())));
    writer->json_keyvalue(
        "address",
        ValueToHexString(reinterpret_cast<int64_t>(immortal->event_loop())));
    writer->json_end();
  }

  writer->json_arrayend();
}

static void PrintResourceUsage(JSONWriter* writer, Immortal* immortal) {
  // Get process uptime in seconds
  uint64_t uptime = (uv_hrtime() - getTimeOrigin()) / (NANOS_PER_SEC);
  if (uptime == 0) uptime = 1;  // avoid division by zero.

  // Process and current thread usage statistics
  struct rusage rusage;
  writer->json_objectstart("resourceUsage");
  if (getrusage(RUSAGE_SELF, &rusage) == 0) {
    double user_cpu =
        rusage.ru_utime.tv_sec + SEC_PER_MICROS * rusage.ru_utime.tv_usec;
    double kernel_cpu =
        rusage.ru_stime.tv_sec + SEC_PER_MICROS * rusage.ru_stime.tv_usec;
    writer->json_keyvalue("userCpuSeconds", user_cpu);
    writer->json_keyvalue("kernelCpuSeconds", kernel_cpu);
    double cpu_abs = user_cpu + kernel_cpu;
    double cpu_percentage = (cpu_abs / uptime) * 100.0;
    writer->json_keyvalue("cpuConsumptionPercent", cpu_percentage);
    writer->json_keyvalue("maxRss", rusage.ru_maxrss * 1024);
    writer->json_objectstart("pageFaults");
    writer->json_keyvalue("IORequired", rusage.ru_majflt);
    writer->json_keyvalue("IONotRequired", rusage.ru_minflt);
    writer->json_objectend();
    writer->json_objectstart("fsActivity");
    writer->json_keyvalue("reads", rusage.ru_inblock);
    writer->json_keyvalue("writes", rusage.ru_oublock);
    writer->json_objectend();
  }
  writer->json_objectend();
}

// Report operating system information.
static void PrintSystemInformation(JSONWriter* writer) {
  env_item_t* envitems;
  int envcount;
  int r = os_environ(&envitems, &envcount);

  writer->json_objectstart("environmentVariables");

  if (r == 0) {
    for (int i = 0; i < envcount; i++)
      writer->json_keyvalue(envitems[i].name, envitems[i].value);

    os_free_environ(envitems, envcount);
  }

  writer->json_objectend();

#ifndef _WIN32
  static struct {
    const char* description;
    int id;
  } rlimit_strings[] = {
    {"core_file_size_blocks", RLIMIT_CORE},
    {"data_seg_size_kbytes", RLIMIT_DATA},
    {"file_size_blocks", RLIMIT_FSIZE},
#if !(defined(_AIX) || defined(__sun))
    {"max_locked_memory_bytes", RLIMIT_MEMLOCK},
#endif
#ifndef __sun
    {"max_memory_size_kbytes", RLIMIT_RSS},
#endif
    {"open_files", RLIMIT_NOFILE},
    {"stack_size_bytes", RLIMIT_STACK},
    {"cpu_time_seconds", RLIMIT_CPU},
#ifndef __sun
    {"max_user_processes", RLIMIT_NPROC},
#endif
#ifndef __OpenBSD__
    {"virtual_memory_kbytes", RLIMIT_AS}
#endif
  };

  writer->json_objectstart("userLimits");
  struct rlimit limit;
  std::string soft, hard;

  for (size_t i = 0; i < arraysize(rlimit_strings); i++) {
    if (getrlimit(rlimit_strings[i].id, &limit) == 0) {
      writer->json_objectstart(rlimit_strings[i].description);

      if (limit.rlim_cur == RLIM_INFINITY)
        writer->json_keyvalue("soft", "unlimited");
      else
        writer->json_keyvalue("soft", limit.rlim_cur);

      if (limit.rlim_max == RLIM_INFINITY)
        writer->json_keyvalue("hard", "unlimited");
      else
        writer->json_keyvalue("hard", limit.rlim_max);

      writer->json_objectend();
    }
  }
  writer->json_objectend();
#endif  // _WIN32

  PrintLoadedLibraries(writer);
}

// Report a list of loaded native libraries.
static void PrintLoadedLibraries(JSONWriter* writer) {
  writer->json_arraystart("sharedObjects");
  std::vector<std::string> modules =
      NativeSymbolDebuggingContext::GetLoadedLibraries();
  for (auto const& module_name : modules) writer->json_element(module_name);
  writer->json_arrayend();
}

// TODO(chengzhong.wcz): Obtain and report the aworker and subcomponent version
// strings.
static void PrintComponentVersions(JSONWriter* writer) {  // NOLINT
  std::stringstream buf;

  writer->json_objectstart("componentVersions");
#define V(key) writer->json_keyvalue(#key, aworker::per_process::metadata.key);
  AWORKER_VERSIONS_KEYS(V)
#undef V
  writer->json_objectend();
}

ReportWatchdog::ReportWatchdog(Immortal* immortal)
    : SignalWatchdog(immortal->watchdog(), SIGUSR2), immortal_(immortal) {}

void ReportWatchdog::OnSignal() {
  immortal_->RequestInterrupt([](Immortal* immortal, InterruptKind kind) {
    HandleScope scope(immortal->isolate());
    TriggerDiagReport(
        immortal->isolate(), immortal, "Signal", "Signal", Local<Value>());
  });
}

}  // namespace report
}  // namespace aworker
