#include "command_parser_group.h"
#include "aworker_binding.h"
#include "immortal.h"

namespace aworker {

const char DEFAULT_PARSER_NAME[] = "$default";

CommandlineParserGroup::CommandlineParserGroup(int default_argc,
                                               char** default_argv) {
  PushCommandlineParser(DEFAULT_PARSER_NAME,
                        default_argc,
                        default_argv,
                        CommandlineParserGroupPushMode::kFull);
}

CommandlineParserGroup::~CommandlineParserGroup() {
  _group.clear();
}

CommandlineParser* CommandlineParserGroup::GetParser(
    const char* parser_name) const {
  for (auto& it : _group) {
    if (it.get()->_parser_name == parser_name) {
      return it.get();
    }
  }

  return nullptr;
}

CommandlineParser* CommandlineParserGroup::GetParser(size_t idx) const {
  if (idx < _group.size()) {
    return _group[idx].get();
  }

  return nullptr;
}

bool CommandlineParserGroup::Evaluate(const char* parser_name) {
  return Evaluate<const char*>(parser_name);
}

bool CommandlineParserGroup::Evaluate(size_t idx) {
  return Evaluate<size_t>(idx);
}

bool CommandlineParserGroup::EvaluateLast() {
  if (_group.empty()) return false;
  return Evaluate(_group.size() - 1);
}

void CommandlineParserGroup::PushCommandlineParser(
    const char* parser_name,
    int argc,
    char** argv,
    CommandlineParserGroupPushMode mode) {
  if (mode == CommandlineParserGroupPushMode::kFull) {
    _group.clear();
  }

  _group.push_back(
      std::make_unique<CommandlineParser>(parser_name, argc, argv));
}

namespace Options {

#define GETTER_HEADER()                                                        \
  Immortal* immortal = Immortal::GetCurrent(info);                             \
  v8::Isolate* isolate = immortal->isolate();                                  \
  CommandlineParserGroup* parser = immortal->commandline_parser();             \
  v8::HandleScope scope(isolate)

#define STRING_GETTER(var)                                                     \
  AWORKER_GETTER(var) {                                                        \
    GETTER_HEADER();                                                           \
    v8::Local<v8::String> ret =                                                \
        v8::String::NewFromUtf8(isolate, parser->var().c_str())                \
            .ToLocalChecked();                                                 \
    info.GetReturnValue().Set(ret);                                            \
  }

#define STRING_HASER(var)                                                      \
  AWORKER_GETTER(has_##var) {                                                  \
    GETTER_HEADER();                                                           \
    v8::Local<v8::Boolean> ret =                                               \
        v8::Boolean::New(isolate, parser->has_##var());                        \
    info.GetReturnValue().Set(ret);                                            \
  }

#define INTEGER_GETTER(var)                                                    \
  AWORKER_GETTER(var) {                                                        \
    GETTER_HEADER();                                                           \
    v8::Local<v8::Integer> ret = v8::Integer::New(isolate, parser->var());     \
    info.GetReturnValue().Set(ret);                                            \
  }

#define BOOLEAN_GETTER(var)                                                    \
  AWORKER_GETTER(var) {                                                        \
    GETTER_HEADER();                                                           \
    v8::Local<v8::Boolean> ret = v8::Boolean::New(isolate, parser->var());     \
    info.GetReturnValue().Set(ret);                                            \
  }

#define V1(var, __, ___, ____, _____, ______, _______) STRING_GETTER(var)
#define V2(var, __, ___, ____, _____, ______, _______) STRING_HASER(var)
CLI_FILEN_ARGS(V1)
CLI_FILEN_ARGS(V2)
#undef V1
#undef V2

#define V1(var, __, ___, ____, _____, ______, _______, ________)               \
  STRING_GETTER(var)
#define V2(var, __, ___, ____, _____, ______, _______, ________)               \
  STRING_HASER(var)
CLI_STRN_ARGS(V1)
CLI_STRN_ARGS(V2)
#undef V1
#undef V2

#define V(var, __, ___, ____, _____, ______) INTEGER_GETTER(var)
CLI_INT_ARGS(V)
#undef V

#define V(var, __, ___, ____, _____) BOOLEAN_GETTER(var)
CLI_LIT_ARGS(V)
#undef V

#define SET_ACCESSOR(var) immortal->SetAccessor(exports, #var, var)

void RawArgs2V8Objects(v8::Local<v8::Object> exports) {
  Immortal* immortal = Immortal::GetCurrent(exports->CreationContext());
#define V(var, __, ___, ____, _____) SET_ACCESSOR(var);
  CLI_LIT_ARGS(V)
#undef V

#define V(var, __, ___, ____, _____, ______) SET_ACCESSOR(var);
  CLI_INT_ARGS(V)
#undef V

#define V1(var, __, ___, ____, _____, ______, _______) SET_ACCESSOR(var);
#define V2(var, __, ___, ____, _____, ______, _______) SET_ACCESSOR(has_##var);
  CLI_FILEN_ARGS(V1)
  CLI_FILEN_ARGS(V2)
#undef V1
#undef V2

#define V1(var, __, ___, ____, _____, ______, _______, ________)               \
  SET_ACCESSOR(var);
#define V2(var, __, ___, ____, _____, ______, _______, ________)               \
  SET_ACCESSOR(has_##var);
  CLI_STRN_ARGS(V1)
  CLI_STRN_ARGS(V2)
#undef V1
#undef V2
}

BOOLEAN_GETTER(mixin_has_agent)
BOOLEAN_GETTER(mixin_inspect)

AWORKER_BINDING(Init) {
  RawArgs2V8Objects(exports);

  SET_ACCESSOR(mixin_inspect);
  SET_ACCESSOR(mixin_has_agent);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
#define V(var, __, ___, ____, _____) registry->Register(var);
  CLI_LIT_ARGS(V)
#undef V

#define V(var, __, ___, ____, _____, ______) registry->Register(var);
  CLI_INT_ARGS(V)
#undef V

#define V1(var, __, ___, ____, _____, ______, _______) registry->Register(var);
#define V2(var, __, ___, ____, _____, ______, _______)                         \
  registry->Register(has_##var);
  CLI_FILEN_ARGS(V1)
  CLI_FILEN_ARGS(V2)
#undef V1
#undef V2

#define V1(var, __, ___, ____, _____, ______, _______, ________)               \
  registry->Register(var);
#define V2(var, __, ___, ____, _____, ______, _______, ________)               \
  registry->Register(has_##var);
  CLI_STRN_ARGS(V1)
  CLI_STRN_ARGS(V2)
#undef V1
#undef V2

  registry->Register(mixin_inspect);
  registry->Register(mixin_has_agent);
}

}  // namespace Options
}  // namespace aworker

AWORKER_BINDING_REGISTER(aworker_options,
                         aworker::Options::Init,
                         aworker::Options::Init)
