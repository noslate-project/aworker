#ifndef SRC_CONTEXTIFY_SCRIPT_H_
#define SRC_CONTEXTIFY_SCRIPT_H_
#include "aworker_binding.h"
#include "base_object.h"
#include "error_handling.h"
#include "immortal.h"

namespace aworker {
namespace contextify {
class ScriptWrap : public BaseObject {
  DEFINE_WRAPPERTYPEINFO();

 protected:
  ScriptWrap(Immortal* immortal,
             v8::Local<v8::Object> object,
             v8::Local<v8::UnboundScript> script);

 public:
  static AWORKER_METHOD(New);
  static AWORKER_METHOD(CreateCachedData);

  SIZE_IN_BYTES(ScriptWrap)
  SET_NO_MEMORY_INFO()

  inline v8::Local<v8::UnboundScript> script() {
    return PersistentToLocal::Strong(script_);
  }

 private:
  v8::Global<v8::UnboundScript> script_;
};

AWORKER_BINDING(ScriptInit);
AWORKER_EXTERNAL_REFERENCE(ScriptInit);

}  // namespace contextify
}  // namespace aworker

#endif  // SRC_CONTEXTIFY_SCRIPT_H_
