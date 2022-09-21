#ifndef SRC_COMMAND_PARSER_H_
#define SRC_COMMAND_PARSER_H_

extern "C" {
#include <argtable3.h>
}

#include <string>
#include <vector>

#include "command_parser-options.h"
#include "v8.h"

namespace aworker {

class CommandlineParserGroup;
class CommandlineParser {
  friend class CommandlineParserGroup;

 public:
  explicit CommandlineParser(const std::string& name, int argc, char** argv);
  virtual ~CommandlineParser();

  void Evaluate();

  inline const char* parser_name() { return _parser_name.c_str(); }

 private:
  void SetupParser();
  void Init(int argc, char** argv);
  void PrintHelp();
  void DumpArgs();

 private:
  std::vector<void*> _argtable;

#define V(var, __, ___, ____, _____)                                           \
 public:                                                                       \
  inline bool var() { return !!_##var->count; }                                \
                                                                               \
 private:                                                                      \
  arg_lit* _##var;
  CLI_LIT_ARGS(V)
#undef V

#define V(var, __, ___, ____, _____, ______)                                   \
 public:                                                                       \
  inline int var() { return *_##var->ival; }                                   \
                                                                               \
 private:                                                                      \
  arg_int* _##var;
  CLI_INT_ARGS(V)
#undef V

#define V(var, __, ___, ____, _____, ______, local_func)                       \
 public:                                                                       \
  inline const char* raw_##var() { return *_raw_##var->filename; }             \
  inline bool has_##var() { return !!_raw_##var->count; }                      \
  inline const std::string& var() { return _##var; }                           \
                                                                               \
 private:                                                                      \
  arg_file* _raw_##var;                                                        \
  std::string _##var;
  CLI_FILEN_ARGS(V)
#undef V

#define V(var, __, ___, ____, _____, ______, _______, local_func)              \
 public:                                                                       \
  inline const char* raw_##var() { return *_##var->sval; }                     \
  inline const std::string var() { return std::string(*_##var->sval); }        \
  inline bool has_##var() { return _##var->count; }                            \
                                                                               \
 private:                                                                      \
  arg_str* _##var;
  CLI_STRN_ARGS(V)
#undef V

 private:
  std::string _parser_name;
  int _nerrors;
  std::string _exec_path;
  std::vector<std::string> _user_argv;
  struct arg_end* _end;
};

}  // namespace aworker

#include "command_parser_group.h"

#endif  // SRC_COMMAND_PARSER_H_
