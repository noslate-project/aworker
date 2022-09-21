#include "util.h"
#include <unistd.h>
#include <string>
#include "debug_utils.h"
#include "error_handling.h"
#include "uv.h"

extern char** environ;

namespace aworker {

using std::string;

void Assert(const AssertionInfo& info) {
  std::string location = SPrintF("%s[%d]: %s:%s%s",
                                 "Aworker",
                                 getpid(),
                                 info.file_line,
                                 info.function,
                                 *info.function ? ":" : "");
  std::string message = SPrintF("Assertion `%s` failed.", info.message);

  FatalError(location.c_str(), message.c_str());
}

void Abort() {
  fflush(stderr);
  abort();
}

int os_environ(env_item_t** envitems, int* count) {
  int i, j, cnt;
  env_item_t* envitem;

  *envitems = NULL;
  *count = 0;

  for (i = 0; environ[i] != NULL; i++) {
    /** empty */
  }

  *envitems = reinterpret_cast<env_item_t*>(calloc(i, sizeof(**envitems)));

  if (*envitems == NULL) return -1;

  for (j = 0, cnt = 0; j < i; j++) {
    char* buf;
    char* ptr;

    if (environ[j] == NULL) break;

    buf = strdup(environ[j]);
    if (buf == NULL) goto fail;

    ptr = strchr(buf, '=');
    if (ptr == NULL) {
      free(buf);
      continue;
    }

    *ptr = '\0';

    envitem = &(*envitems)[cnt];
    envitem->name = buf;
    envitem->value = ptr + 1;

    cnt++;
  }

  *count = cnt;
  return 0;

fail:
  for (i = 0; i < cnt; i++) {
    envitem = &(*envitems)[cnt];
    free(envitem->name);
  }
  free(*envitems);

  *envitems = NULL;
  *count = 0;
  return -1;
}

void os_free_environ(env_item_t* envitems, int count) {
  int i;

  for (i = 0; i < count; i++) {
    free(envitems[i].name);
  }

  free(envitems);
}

int ReadFileSync(std::string* result, const char* path) {
  uv_fs_t req;
  auto defer_req = OnScopeLeave([&req]() { uv_fs_req_cleanup(&req); });

  uv_file file = uv_fs_open(nullptr, &req, path, O_RDONLY, 0, nullptr);
  if (req.result < 0) {
    return req.result;
  }
  uv_fs_req_cleanup(&req);

  auto defer_close = OnScopeLeave([file]() {
    uv_fs_t close_req;
    CHECK_EQ(0, uv_fs_close(nullptr, &close_req, file, nullptr));
    uv_fs_req_cleanup(&close_req);
  });

  *result = std::string("");
  char buffer[4096];
  uv_buf_t buf = uv_buf_init(buffer, sizeof(buffer));

  while (true) {
    const int r =
        uv_fs_read(nullptr, &req, file, &buf, 1, result->length(), nullptr);
    if (req.result < 0) {
      return req.result;
    }
    uv_fs_req_cleanup(&req);
    if (r <= 0) {
      break;
    }
    result->append(buf.base, r);
  }
  return 0;
}

double GetCurrentTimeInMicroseconds() {
  constexpr double kMicrosecondsPerSecond = 1e6;
  uv_timeval64_t tv;
  CHECK_EQ(0, uv_gettimeofday(&tv));
  return kMicrosecondsPerSecond * tv.tv_sec + tv.tv_usec;
}

}  // namespace aworker
