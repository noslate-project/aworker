#ifndef SRC_COMMAND_PARSER_GROUP_H_
#define SRC_COMMAND_PARSER_GROUP_H_

#include "command_parser.h"
#include "util.h"

namespace aworker {

extern const char DEFAULT_PARSER_NAME[];
enum class CommandlineParserGroupPushMode {
  kFull,
  kIncremental,
};

// TODO(kaidi.zkd): refactor to use as less as possible macros.
class CommandlineParserGroup {
 public:
  CommandlineParserGroup(int default_argc, char** default_argv);
  ~CommandlineParserGroup();

  CommandlineParser* GetParser(const char* parser_name) const;
  CommandlineParser* GetParser(size_t idx) const;
  bool Evaluate(const char* parser_name);
  bool Evaluate(size_t idx);
  bool EvaluateLast();
  template <typename T>
  bool Evaluate(T key) {
    CommandlineParser* parser = GetParser(key);
    if (parser == nullptr) return false;
    parser->Evaluate();
    return true;
  }

  void PushCommandlineParser(const char* parser_name,
                             int argc,
                             char** argv,
                             CommandlineParserGroupPushMode mode =
                                 CommandlineParserGroupPushMode::kIncremental);

  inline bool mixin_has_agent() { return has_agent() || ref_agent(); }
  inline bool mixin_inspect() {
    return inspect() || inspect_brk() || inspect_brk_aworker();
  }

  // Process of fetching each option:
  //   1. Traverse all `CommandlineParser` to check if that option exists in
  //      that `CommandlineParser` instance;
  //   2.1. If option exists, return hit `CommandlineParser`'s related option;
  //   2.2. If option not exists, traverse next `CommandlineParser` instance;
  //   3. If option doesn't exist in any `CommandlineParser` instance, return a
  //      default value. (e.g. first `CommandlineParser` instance' value)
#define TRAVERSE_RETURN(var, found_expr, not_found_expr)                       \
  CHECK_GT(_group.size(), 0);                                                  \
  for (auto rit = _group.rbegin(); rit != _group.rend(); rit++) {              \
    if ((*rit)->_##var->count) return (found_expr);                            \
  }                                                                            \
  return (not_found_expr);

#define FIRST (_group[0].get())

#define V(var, __, ___, ____, _____)                                           \
 public:                                                                       \
  inline bool var() { TRAVERSE_RETURN(var, true, false); }
  CLI_LIT_ARGS(V)
#undef V

#define V(var, __, ___, ____, _____, ______)                                   \
 public:                                                                       \
  inline int var() {                                                           \
    TRAVERSE_RETURN(var, *((*rit)->_##var->ival), *(FIRST->_##var->ival));     \
  }
  CLI_INT_ARGS(V)
#undef V

#define V(var, __, ___, ____, _____, ______, local_func)                       \
 public:                                                                       \
  inline const char* raw_##var() {                                             \
    TRAVERSE_RETURN(raw_##var,                                                 \
                    *((*rit)->_raw_##var->filename),                           \
                    *(FIRST->_raw_##var->filename));                           \
  }                                                                            \
  inline bool has_##var() { TRAVERSE_RETURN(raw_##var, true, false); }         \
  inline const std::string var() {                                             \
    TRAVERSE_RETURN(raw_##var, (*rit)->_##var, FIRST->_##var);                 \
  }
  CLI_FILEN_ARGS(V)
#undef V

#define V(var, __, ___, ____, _____, ______, _______, local_func)              \
 public:                                                                       \
  inline const char* raw_##var() {                                             \
    TRAVERSE_RETURN(var, *((*rit)->_##var->sval), *(FIRST->_##var->sval));     \
  }                                                                            \
  inline const std::string var() {                                             \
    TRAVERSE_RETURN(var,                                                       \
                    std::string(*((*rit)->_##var->sval)),                      \
                    std::string(*(FIRST->_##var->sval)));                      \
  }                                                                            \
  inline bool has_##var() { TRAVERSE_RETURN(var, true, false); }
  CLI_STRN_ARGS(V)
#undef V

#undef TRAVERSE_RETURN

 private:
  std::vector<std::unique_ptr<CommandlineParser>> _group;
};

}  // namespace aworker

#endif  // SRC_COMMAND_PARSER_GROUP_H_
