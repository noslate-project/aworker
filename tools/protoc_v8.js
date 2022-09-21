'use strict';

const arg = require('arg');
const protobuf = require('protobufjs');
const fs = require('fs');

function main(argv) {
  const opt = arg({
    '--include': String,
    '--out': String,

    '-I': '--include',
  }, {
    argv,
    stopAtPositional: true,
  });

  const inputFile = opt._[0];

  const root = new protobuf.Root();
  root.loadSync(inputFile, {
    keepCase: true,
  });
  let result = `#pragma once
#include "v8.h"
#include "util.h"
#include "proto_helper.h"
`;

  {
    const toBeTraversed = [[[], root ]];
    while (toBeTraversed.length !== 0) {
      const [ namespaces, item ] = toBeTraversed.pop();
      if (item.nested) {
        toBeTraversed.push(...Object.values(item.nested)
          .map(nested => [[ ...namespaces, item.name ], nested ]));
      }
      if (!(item instanceof protobuf.Type)) {
        continue;
      }
      const normalizedNamespaces = namespaces.filter(it => it !== '');

      const toValueConst = ToValueIsConst(item) ? 'const ' : '';
      const canonicalName = `${normalizedNamespaces.join('::')}::${item.name}`;
      const typeDeclaration = `class ${item.name}Helper {
public:
  static inline v8::Local<v8::Value> ToValue(${toValueConst}${canonicalName}* item, v8::Local<v8::Context> context);
  static inline void FromValue(v8::Local<v8::Context> context, v8::Local<v8::Value> value, ${canonicalName}* target);
};`;

      const namespaceResult = `${normalizedNamespaces.reduceRight((content, namespace) => `namespace ${namespace} {
${content}
} // namespace ${namespace}`, typeDeclaration)}
`;
      result += namespaceResult;
    }
  }

  {
    const toBeTraversed = [[[], root ]];
    while (toBeTraversed.length !== 0) {
      const [ namespaces, item ] = toBeTraversed.pop();
      if (item.nested) {
        toBeTraversed.push(...Object.values(item.nested)
          .map(nested => [[ ...namespaces, item.name ], nested ]));
      }
      if (!(item instanceof protobuf.Type)) {
        continue;
      }
      const normalizedNamespaces = namespaces.filter(it => it !== '');

      const toValueConst = ToValueIsConst(item) ? 'const ' : '';
      const canonicalName = `${normalizedNamespaces.join('::')}::${item.name}`;
      const typeDefinition = `inline v8::Local<v8::Value> ${item.name}Helper::ToValue(${toValueConst}${canonicalName}* item, v8::Local<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Object> value = v8::Object::New(isolate);
${Object.values(item.fields)
    .map(field =>
      ToValue(field, root)
        .split('\n')
        .map(it => '    ' + it)
        .join('\n')
    ).join('\n')}
  return value;
}

inline void ${item.name}Helper::FromValue(v8::Local<v8::Context> context, v8::Local<v8::Value> __value, ${canonicalName}* target) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Object> value = __value.As<v8::Object>();
${Object.values(item.fields)
    .map(field =>
      FromValue(field, root)
        .split('\n')
        .map(it => '    ' + it)
        .join('\n')
    ).join('\n')}
}`;

      const namespaceResult = `${normalizedNamespaces.reduceRight((content, namespace) => `namespace ${namespace} {
${content}
} // namespace ${namespace}`, typeDefinition)}
`;
      result += namespaceResult;
    }
  }

  if (opt['--out']) {
    fs.writeFileSync(opt['--out'], result, 'utf8');
  } else {
    console.log(result);
  }
}

function ToValue(field, root) {
  const { name, type } = field;
  let result = `// ${name}\n`;
  if (field.repeated) {
    result += `{
  v8::Local<v8::Array> ${name}_arr = v8::Array::New(isolate, item->${name}_size());
  for (int idx = 0; idx < item->${name}_size(); idx++) {
    const auto& nested_${name} = item->${name}(idx);
    ${ConcreteToValue(name, type, root)}
    ${name}_arr->Set(context, idx, _${name}).Check();
  }
  value->Set(context, aworker::OneByteString(isolate, "${name}"), ${name}_arr).Check();
}`;
  } else if (type === 'bytes') {
    result += `{
  auto* nested_${name} = item->release_${name}();
  ${ConcreteToValue(name, type, root)}
  value->Set(context, aworker::OneByteString(isolate, "${name}"), _${name}).Check();
}`;
  } else {
    result += `{
  const auto& nested_${name} = item->${name}();
  ${ConcreteToValue(name, type, root)}
  value->Set(context, aworker::OneByteString(isolate, "${name}"), _${name}).Check();
}`;
  }
  return result;
}

function ConcreteToValue(name, type, root) {
  let result = '';
  switch (type) {
    // value type
    case 'fixed32':
    case 'int32': {
      result += `v8::Local<v8::Value> _${name} = v8::Number::New(isolate, nested_${name});`;
      break;
    }
    case 'bytes': {
      result += `
v8::Local<v8::Value> _${name} = aworker_proto::BytesHelper::ToValue(nested_${name}, context);
`;
      break;
    }
    // reference type
    default: {
      const canonicalName = getTypeCanonicalName(type, root);
      if (canonicalName == null) {
        result += `// TODO: ${type}`;
      } else {
        const toValueIsConst = root.lookup(type) ? ToValueIsConst(root.lookup(type)) : true;
        const nameRef = toValueIsConst ? `&nested_${name}` : `const_cast<${canonicalName}*>(&nested_${name})`;
        result += `
  v8::Local<v8::Value> _${name} = ${canonicalName}Helper::ToValue(${nameRef}, context);
`;
      }
    }
  }
  return result;
}

function FromValue(field, root) {
  const { name, type } = field;
  let result = `// ${name}\n`;
  result += `if (value->Has(context, aworker::OneByteString(isolate, "${name}")).FromMaybe(false))`;
  if (field.repeated) {
    result += `{
  v8::Local<v8::Array> ${name}_arr = value->Get(context, aworker::OneByteString(isolate, "${name}"))
    .ToLocalChecked()
    .As<v8::Array>();
  for (uint32_t idx = 0; idx < ${name}_arr->Length(); idx++) {
    v8::Local<v8::Value> _${name} = ${name}_arr->Get(context, idx).ToLocalChecked();
    ${ConcreteFromValue(name, type, root, true)}
  }
}`;
  } else {
    result += `{
  v8::Local<v8::Value> _${name} = value->Get(context, aworker::OneByteString(isolate, "${name}"))
    .ToLocalChecked();
  ${ConcreteFromValue(name, type, root)}
}`;
  }
  return result;
}

function ConcreteFromValue(name, type, root, repeating = false) {
  let result = '';
  switch (type) {
    // value type
    case 'fixed32':
    case 'int32': {
      result += `target->set_${name}(_${name}->Int32Value(context).ToChecked());`;
      break;
    }
    // reference type
    default: {
      const canonicalName = getTypeCanonicalName(type, root);
      if (canonicalName == null) {
        result += `// TODO: ${type}`;
      } else {
        result += `auto* nested_${name} = ${repeating ? `target->add_${name}()` : `target->mutable_${name}()`};
  ${canonicalName}Helper::FromValue(context, _${name}, nested_${name});`;
      }
    }
  }
  return result;
}

function ToValueIsConst(Type) {
  return Object.values(Type.fields).find(it => it.type === 'bytes') == null;
}

const PrimitiveTypeNames = {
  bytes: 'aworker_proto::Bytes',
  string: 'aworker_proto::String',
};
function getTypeCanonicalName(typeName, root) {
  if (PrimitiveTypeNames[typeName]) {
    return PrimitiveTypeNames[typeName];
  }
  const Type = root.lookup(typeName);
  if (Type == null) {
    return null;
  }
  const { name } = Type;
  let parent = Type.parent;
  const namespaces = [];
  while (parent != null && !(parent instanceof protobuf.Root)) {
    namespaces.unshift(parent.name);
    parent = parent.parent;
  }
  return `${namespaces.join('::')}::${name}`;
}

main(process.argv.slice(2));
