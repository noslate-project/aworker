#ifndef SRC_COMMAND_PARSER_INL_H_
#define SRC_COMMAND_PARSER_INL_H_

#include "command_parser.h"
#include "immortal.h"
#include "v8.h"

#define ARG_LIT0(var, _long, _short, desc)                                     \
  _argtable.push_back(var = arg_lit0(_short, _long, desc))
#define ARG_INT0(var, _long, _short, meta, desc)                               \
  _argtable.push_back(var = arg_int0(_short, _long, meta, desc))
#define ARG_STRN(var, _long, _short, meta, min, max, desc)                     \
  _argtable.push_back(var = arg_strn(_short, _long, meta, min, max, desc))
#define ARG_FILEN(var, _long, _short, meta, min, max, desc)                    \
  _argtable.push_back(var = arg_filen(_short, _long, meta, min, max, desc))
#define ARG_END() _argtable.push_back(_end = arg_end(20))

namespace aworker {

void CommandlineParser::DumpArgs() {
#define V(var, _long, __, ___, ____)                                           \
  printf("%s: %s\n", _long, var() ? "true" : "false");
  CLI_LIT_ARGS(V)
#undef V

#define V(var, _long, __, ___, ____, _____) printf("%s: %d\n", _long, var());
  CLI_INT_ARGS(V)
#undef V

#define V(var, _long, __, ___, ____, _____, ______, _______)                   \
  printf("%s: %s\n", _long, var().c_str());
  CLI_STRN_ARGS(V)
#undef V

#define V(var, _long, meta, __, ___, ____, _____)                              \
  printf("%s: %s\n", _long == nullptr ? meta : _long, var().c_str());
  CLI_FILEN_ARGS(V)
#undef V
}

void CommandlineParser::SetupParser() {
#define V(var, _long, _short, desc, __)                                        \
  { ARG_LIT0(_##var, _long, _short, desc); }
  CLI_LIT_ARGS(V)
#undef V

#define V(var, _long, meta, desc, dflt, __)                                    \
  {                                                                            \
    ARG_INT0(_##var, _long, nullptr, meta, desc);                              \
    _##var->ival[0] = dflt;                                                    \
  }
  CLI_INT_ARGS(V)
#undef V

#define V(var, _long, meta, min, max, desc, dflt, __)                          \
  {                                                                            \
    ARG_STRN(_##var, _long, nullptr, meta, min, max, desc);                    \
    _##var->sval[0] = dflt;                                                    \
  }
  CLI_STRN_ARGS(V)
#undef V

#define V(var, _long, meta, min, max, desc, __)                                \
  ARG_FILEN(_raw_##var, _long, nullptr, meta, min, max, desc);
  CLI_FILEN_ARGS(V)
#undef V

  ARG_END();
}

}  // namespace aworker

#endif  // SRC_COMMAND_PARSER_INL_H_
