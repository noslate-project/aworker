#ifndef SRC_UTILS_SCOPE_H_
#define SRC_UTILS_SCOPE_H_

#include "cwalk.h"

namespace aworker {

class CWalkPathStyleScope {
 public:
  inline explicit CWalkPathStyleScope(const char* path)
      : CWalkPathStyleScope(cwk_path_guess_style(path)) {}

  inline explicit CWalkPathStyleScope(enum cwk_path_style style)
      : CWalkPathStyleScope() {
    cwk_path_set_style(style);
  }

  inline CWalkPathStyleScope() : _previous_style(cwk_path_get_style()) {}

  inline void SetStyle(enum cwk_path_style style) { cwk_path_set_style(style); }

  inline void SetStyle(const char* path) {
    SetStyle(cwk_path_guess_style(path));
  }

  inline ~CWalkPathStyleScope() {
    if (_previous_style == kDefaultPathStyle) {
      cwk_path_set_style(kDefaultPathStyle);
    } else {
      cwk_path_set_style(_previous_style);
    }
  }

 private:
  static enum cwk_path_style const kDefaultPathStyle = CWK_STYLE_UNIX;
  enum cwk_path_style _previous_style;
};

}  // namespace aworker

#endif  // SRC_UTILS_SCOPE_H_
