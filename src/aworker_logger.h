#ifndef SRC_AWORKER_LOGGER_H_
#define SRC_AWORKER_LOGGER_H_
#include <stdio.h>
#include <string.h>
#include <time.h>

// Environs:
// - AWORKER_LOGLEVEL: 'info', 'error'.
//
// Logging format:
// [D] 2022-06-17 13:04:07:007 (ipc_delegate_impl.cc:118) got socket

namespace aworker {

enum class LogLevel {
  kDebug = 0,
  kInfo = 1,
  kError = 2,
};

LogLevel GetLogLevel();
void RefreshLogLevel();

inline bool LogLevelEnabled(LogLevel level) {
  return static_cast<size_t>(level) >= static_cast<size_t>(GetLogLevel());
}

}  // namespace aworker

#define __FILENAME__                                                           \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define ___FILENAME___ __FILENAME__

#define LOG_LEVEL_INFO_TAG "[I]"
#define LOG_LEVEL_ERROR_TAG "[E]"
#define LOG_LEVEL_DEBUG_TAG "[D]"

#define _LOG(fd, level, level_tag, filename, line, format, ...)                \
  if (aworker::LogLevelEnabled(level)) {                                       \
    time_t t = time(NULL);                                                     \
    struct tm tm;                                                              \
    localtime_r(&t, &tm);                                                      \
    struct timespec ts;                                                        \
    clock_gettime(CLOCK_REALTIME, &ts);                                        \
    fprintf(fd,                                                                \
            level_tag " %d-%02d-%02d %02d:%02d:%02d:%03ld (%s:%d) " format     \
                      "\n",                                                    \
            tm.tm_year + 1900,                                                 \
            tm.tm_mon + 1,                                                     \
            tm.tm_mday,                                                        \
            tm.tm_hour,                                                        \
            tm.tm_min,                                                         \
            tm.tm_sec,                                                         \
            ts.tv_nsec / 1000000,                                              \
            filename,                                                          \
            line,                                                              \
            __VA_ARGS__);                                                      \
  }

#define __LOG(fd, level, level_tag, format, ...)                               \
  if (aworker::LogLevelEnabled(level)) {                                       \
    time_t t = time(NULL);                                                     \
    struct tm tm;                                                              \
    localtime_r(&t, &tm);                                                      \
    struct timespec ts;                                                        \
    clock_gettime(CLOCK_REALTIME, &ts);                                        \
    fprintf(fd,                                                                \
            level_tag " %d-%02d-%02d %02d:%02d:%02d:%03ld (%s:%d) " format     \
                      "\n",                                                    \
            tm.tm_year + 1900,                                                 \
            tm.tm_mon + 1,                                                     \
            tm.tm_mday,                                                        \
            tm.tm_hour,                                                        \
            tm.tm_min,                                                         \
            tm.tm_sec,                                                         \
            ts.tv_nsec / 1000000,                                              \
            __FILENAME__,                                                      \
            __LINE__,                                                          \
            __VA_ARGS__);                                                      \
  }

#define __LOG_4_ARGS(fd, level, level_tag, format, ...)                        \
  if (aworker::LogLevelEnabled(level)) {                                       \
    time_t t = time(NULL);                                                     \
    struct tm tm;                                                              \
    localtime_r(&t, &tm);                                                      \
    struct timespec ts;                                                        \
    clock_gettime(CLOCK_REALTIME, &ts);                                        \
    fprintf(fd,                                                                \
            level_tag " %d-%02d-%02d %02d:%02d:%02d:%03ld (%s:%d) " format     \
                      "\n",                                                    \
            tm.tm_year + 1900,                                                 \
            tm.tm_mon + 1,                                                     \
            tm.tm_mday,                                                        \
            tm.tm_hour,                                                        \
            tm.tm_min,                                                         \
            tm.tm_sec,                                                         \
            ts.tv_nsec / 1000000,                                              \
            __FILENAME__,                                                      \
            __LINE__);                                                         \
  }

#define GET_18TH_ARG(arg1,                                                     \
                     arg2,                                                     \
                     arg3,                                                     \
                     arg4,                                                     \
                     arg5,                                                     \
                     arg6,                                                     \
                     arg7,                                                     \
                     arg8,                                                     \
                     arg9,                                                     \
                     arg10,                                                    \
                     arg11,                                                    \
                     arg12,                                                    \
                     arg13,                                                    \
                     arg14,                                                    \
                     arg15,                                                    \
                     arg16,                                                    \
                     arg17,                                                    \
                     arg18,                                                    \
                     ...)                                                      \
  arg18

#define LOG_MACRO_CHOOSER(...)                                                 \
  GET_18TH_ARG(__VA_ARGS__,                                                    \
               __LOG,                                                          \
               __LOG,                                                          \
               __LOG,                                                          \
               __LOG,                                                          \
               __LOG,                                                          \
               __LOG,                                                          \
               __LOG,                                                          \
               __LOG,                                                          \
               __LOG,                                                          \
               __LOG,                                                          \
               __LOG,                                                          \
               __LOG,                                                          \
               __LOG,                                                          \
               __LOG,                                                          \
               __LOG,                                                          \
               __LOG,                                                          \
               __LOG_4_ARGS, )

#define ILOG(...)                                                              \
  LOG_MACRO_CHOOSER(__VA_ARGS__)                                               \
  (stdout, aworker::LogLevel::kInfo, LOG_LEVEL_INFO_TAG, __VA_ARGS__)

#define ELOG(...)                                                              \
  LOG_MACRO_CHOOSER(__VA_ARGS__)                                               \
  (stderr, aworker::LogLevel::kError, LOG_LEVEL_ERROR_TAG, __VA_ARGS__)

#ifdef DEBUG
#define DLOG(...)                                                              \
  LOG_MACRO_CHOOSER(__VA_ARGS__)                                               \
  (stdout, aworker::LogLevel::kDebug, LOG_LEVEL_DEBUG_TAG, __VA_ARGS__)
#else
#define DLOG(...)
#endif

#endif  // SRC_AWORKER_LOGGER_H_
