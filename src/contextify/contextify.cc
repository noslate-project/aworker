#ifndef SRC_CONTEXTIFY_CONTEXTIFY_CC_
#define SRC_CONTEXTIFY_CONTEXTIFY_CC_
#include "aworker_binding.h"
#include "context.h"
#include "immortal.h"
#include "script.h"

namespace aworker {
namespace contextify {

using v8::FunctionTemplate;
using v8::Local;
using v8::Object;

AWORKER_BINDING(Init) {
  ContextInit(exports, context, immortal);
  ScriptInit(exports, context, immortal);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  ContextInit(registry);
  ScriptInit(registry);
}

}  // namespace contextify
}  // namespace aworker

#endif  // SRC_CONTEXTIFY_CONTEXTIFY_CC_

AWORKER_BINDING_REGISTER(contextify,
                         aworker::contextify::Init,
                         aworker::contextify::Init)
