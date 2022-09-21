#include <cwalk.h>
#include <limits.h>
#include <iostream>

#include "aworker_binding.h"
#include "command_parser-inl.h"
#include "command_parser.h"
#include "util.h"

namespace aworker {

using std::string;
using v8::Boolean;
using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;

const char PROG_NAME[] = "aworker";

CommandlineParser::CommandlineParser(const string& name, int argc, char** argv)
    : _parser_name(name) {
  SetupParser();
  Init(argc, argv);
}

CommandlineParser::~CommandlineParser() {
  arg_freetable(_argtable.data(), _argtable.size());
}

void CommandlineParser::Init(int argc, char** argv) {
  if (argc == 0) {
    return;
  }
  _exec_path = string(argv[0]);
  int user_argv_idx = 1;
  for (; user_argv_idx < argc; user_argv_idx++) {
    if (argv[user_argv_idx][0] != '-') {
      break;
    }
  }
  /** exclude the script filename from user argv */
  user_argv_idx++;
  for (int idx = user_argv_idx; idx < argc; idx++) {
    _user_argv.push_back(string(argv[idx]));
  }

  _nerrors = arg_parse(argc, argv, _argtable.data());
}

void CommandlineParser::Evaluate() {
  if (_nerrors > 0) {
    arg_print_errors(stderr, _end, "aworker");
    PrintHelp();
    exit(4);
  }

  if (help()) {
    PrintHelp();
    exit(0);
  }

  if (mode() != "normal" && mode() != "seed" && mode() != "seed-userland") {
    fprintf(stderr,
            "%s: run mode should be normal or seed, but got %s.\n",
            PROG_NAME,
            mode().c_str());
    PrintHelp();
    exit(4);
  }

  // transform FILENs into real path name
  {
    string cwd = aworker::cwd();
    char buffer[PATH_MAX];
    char* temp;

#define V(var, __, ___, ____, _____, ______, _______)                          \
  if (*(temp = const_cast<char*>(raw_##var())) != '\0') {                      \
    cwk_path_get_absolute(cwd.c_str(), temp, buffer, sizeof(buffer));          \
    _##var = buffer;                                                           \
  }
    CLI_FILEN_ARGS(V)
#undef V
  }

#define V(cond, rise)                                                          \
  if (has_##cond()) {                                                          \
    _##rise->count = 1;                                                        \
  }
  CLI_IMPLIES(V)
#undef V

  if (dump_arguments()) {
    DumpArgs();
    exit(0);
  }
}

void CommandlineParser::PrintHelp() {
  printf("Usage: %s", PROG_NAME);
  arg_print_syntax(stdout, _argtable.data(), "\n");
  arg_print_glossary(stdout, _argtable.data(), "  %-25s %s\n");
}
}  // namespace aworker
