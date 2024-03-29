#ifndef SRC_BINDING_INTERNAL_AWORKER_I18N_H_
#define SRC_BINDING_INTERNAL_AWORKER_I18N_H_

#include <unicode/ucnv.h>
#include <v8.h>
#include <string>

#include "aworker_binding.h"
#include "base_object.h"
#include "snapshot/snapshotable.h"

namespace aworker {
namespace i18n {

using std::string;

using v8::FunctionCallbackInfo;
using v8::Global;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Value;

enum ConverterFlags {
  CONVERTER_FLAGS_NONE = 0,
  CONVERTER_FLAGS_FATAL = 1 << 0,
  CONVERTER_FLAGS_IGNORE_BOM = 1 << 1,
  CONVERTER_FLAGS_STREAM = 1 << 2,
};

class ConverterObject : public SnapshotableObject {
  DEFINE_WRAPPERTYPEINFO();
  SIZE_IN_BYTES(ConverterObject)
  SET_NO_MEMORY_INFO()

 public:
  enum InternalFields {
    kName = BaseObject::kInternalFieldCount,
    kInternalFieldCount,
  };

  static AWORKER_METHOD(New);
  static AWORKER_METHOD(Decode);
  static AWORKER_METHOD(Encode);

  SERIALIZABLE_OBJECT_METHODS();

 protected:
  explicit ConverterObject(Immortal* immortal,
                           Local<Object> object,
                           Local<v8::String> name);
  ~ConverterObject();

  size_t min_char_size() const;

  bool ignore_bom() const {
    return (flags_ & CONVERTER_FLAGS_IGNORE_BOM) == CONVERTER_FLAGS_IGNORE_BOM;
  }

 private:
  UConverter* conv_;
  int flags_ = CONVERTER_FLAGS_NONE;
};

}  // namespace i18n
}  // namespace aworker

#endif  // SRC_BINDING_INTERNAL_AWORKER_I18N_H_
