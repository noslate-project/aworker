#include "module_wrap.h"
#include "util.h"

namespace aworker {

using v8::Array;
using v8::Context;
using v8::FixedArray;
using v8::Function;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Module;
using v8::ModuleRequest;
using v8::Object;
using v8::Promise;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::String;
using v8::Value;

const WrapperTypeInfo ModuleWrap::wrapper_type_info_{
    "module_wrap",
};

MaybeLocal<Promise>
ModuleWrap::HostImportModuleDynamicallyWithImportAssertionsCallback(
    Local<Context> context,
    Local<v8::ScriptOrModule> referrer,
    Local<String> specifier,
    Local<FixedArray> import_assertions) {
  Immortal* immortal = Immortal::GetCurrent(context);
  Local<Value> global = context->Global();

  Local<Value> resource_name = referrer->GetResourceName();
  Local<Value> argv[] = {resource_name, specifier};
  MaybeLocal<Value> maybe_value =
      immortal->module_wrap_dynamic_import_function()->Call(
          context, global, arraysize(argv), argv);

  if (maybe_value.IsEmpty()) {
    return MaybeLocal<Promise>();
  }
  Local<Value> value = maybe_value.ToLocalChecked();
  CHECK(value->IsPromise());
  return value.As<Promise>();
}

void ModuleWrap::HostInitializeImportMetaObjectCallback(Local<Context> context,
                                                        Local<Module> module,
                                                        Local<Object> meta) {
  Immortal* immortal = Immortal::GetCurrent(context);
  ModuleWrap* wrap = ModuleWrap::GetFromModule(immortal, module);
  Local<Value> global = context->Global();
  Local<Value> argv[] = {wrap->object(), meta};
  immortal->module_wrap_init_import_meta_function()
      ->Call(context, global, arraysize(argv), argv)
      .ToLocalChecked();
}

MaybeLocal<Module> ModuleWrap::ResolveCallback(
    Local<Context> context,
    Local<String> specifier,
    Local<FixedArray> import_assertions,
    Local<Module> referrer) {
  Immortal* immortal = Immortal::GetCurrent(context);
  Isolate* isolate = immortal->isolate();
  ModuleWrap* wrap = ModuleWrap::GetFromModule(immortal, referrer);
  CHECK(wrap->_linked);

  String::Utf8Value specifier_utf8(isolate, specifier);
  std::string specifier_std = *specifier_utf8;

  auto it = wrap->_resolve_cache.find(specifier_std);
  if (it == wrap->_resolve_cache.end()) {
    return MaybeLocal<Module>();
  }
  CHECK(!it->second.IsEmpty());
  Local<Promise> promise = PersistentToLocal::Strong(it->second);
  CHECK_EQ(promise->State(), Promise::PromiseState::kFulfilled);
  Local<Object> obj = promise->Result().As<Object>();
  ModuleWrap* request_wrap;
  ASSIGN_OR_RETURN_UNWRAP(&request_wrap, obj, MaybeLocal<Module>());
  return request_wrap->module();
}

ModuleWrap* ModuleWrap::GetFromModule(Immortal* immortal,
                                      Local<Module> module) {
  auto it = immortal->hash_to_module_map.find(module->GetIdentityHash());
  if (it != immortal->hash_to_module_map.end()) {
    return it->second;
  }
  return nullptr;
}

ModuleWrap::ModuleWrap(Immortal* immortal,
                       Local<Object> object,
                       Local<Module> module,
                       Local<String> url)
    : BaseObject(immortal, object) {
  _module.Reset(immortal->isolate(), module);
  immortal->hash_to_module_map[module->GetIdentityHash()] = this;
}

ModuleWrap::~ModuleWrap() {
  Local<Module> mod = PersistentToLocal::Strong<Module>(_module);
  auto it = immortal()->hash_to_module_map.find(mod->GetIdentityHash());
  if (it != immortal()->hash_to_module_map.end()) {
    immortal()->hash_to_module_map.erase(it);
  }
}

AWORKER_METHOD(ModuleWrap::New) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  Local<String> code = info[0].As<String>();
  Local<String> filename = info[1].As<String>();
  Local<Integer> line_offset = info[2].As<Integer>();
  Local<Integer> column_offset = info[3].As<Integer>();

  ScriptOrigin origin(isolate,
                      filename,
                      line_offset->Value(),    // line offset
                      column_offset->Value(),  // column offset
                      false,                   // is cross origin
                      -1,                      // script id
                      Local<Value>(),          // source map URL
                      false,                   // is opaque (?)
                      false,                   // is wasm
                      true);                   // is module

  ScriptCompiler::CompileOptions compile_options =
      ScriptCompiler::kNoCompileOptions;
  ScriptCompiler::Source source(code, origin, /* cached data */ nullptr);

  MaybeLocal<Module> maybe_module =
      ScriptCompiler::CompileModule(isolate, &source, compile_options);
  if (maybe_module.IsEmpty()) {
    /** parse failed */
    return;
  }
  Local<Module> mod = maybe_module.ToLocalChecked();
  new ModuleWrap(
      immortal, info.This(), mod, /* TODO(chengzhong.wcz): url? */ filename);
}

AWORKER_METHOD(ModuleWrap::Link) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal->context();
  Local<Value> global = context->Global();

  Local<Object> obj = info.This();
  ModuleWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, obj);
  Local<Module> mod = wrap->module();

  Local<Function> resolver = info[0].As<Function>();

  Local<FixedArray> module_requests = mod->GetModuleRequests();
  std::vector<Local<Value>> promises(module_requests->Length());
  for (int idx = 0; idx < module_requests->Length(); idx++) {
    Local<ModuleRequest> module_request =
        module_requests->Get(context, idx).As<ModuleRequest>();
    Local<String> specifier = module_request->GetSpecifier();
    String::Utf8Value specifier_utf8(isolate, specifier);
    std::string specifier_std = *specifier_utf8;

    Local<Value> argv[] = {obj, specifier};
    Local<Value> value =
        resolver->Call(context, global, arraysize(argv), argv).ToLocalChecked();
    CHECK(value->IsPromise());
    Local<Promise> resolve_promise = value.As<Promise>();
    wrap->_resolve_cache[specifier_std].Reset(isolate, resolve_promise);
    promises[idx] = resolve_promise;
  }
  wrap->_linked = true;

  info.GetReturnValue().Set(
      Array::New(isolate, promises.data(), promises.size()));
}

AWORKER_METHOD(ModuleWrap::Instantiate) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal->context();

  Local<Object> obj = info.This();
  ModuleWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, obj);
  Local<Module> mod = wrap->module();

  USE(mod->InstantiateModule(context, ResolveCallback));
}

AWORKER_METHOD(ModuleWrap::GetNamespace) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  Local<Object> obj = info.This();
  ModuleWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, obj);
  Local<Module> mod = wrap->module();

  info.GetReturnValue().Set(mod->GetModuleNamespace());
}

AWORKER_METHOD(ModuleWrap::SetImportModuleDynamicallyCallback) {
  Immortal* immortal = Immortal::GetCurrent(info);
  CHECK(info[0]->IsFunction());
  Local<Function> callback = info[0].As<Function>();
  immortal->set_module_wrap_dynamic_import_function(callback);
}

AWORKER_METHOD(ModuleWrap::SetInitializeImportMetaObjectCallback) {
  Immortal* immortal = Immortal::GetCurrent(info);
  CHECK(info[0]->IsFunction());
  Local<Function> callback = info[0].As<Function>();
  immortal->set_module_wrap_init_import_meta_function(callback);
}

void ModuleWrap::Init(Immortal* immortal, v8::Local<v8::Object> exports) {
  auto isolate = immortal->isolate();
  HandleScope scope(isolate);
  auto context = immortal->context();

  Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, ModuleWrap::New);
  tpl->Inherit(BaseObject::GetConstructorTemplate(immortal));
  tpl->InstanceTemplate()->SetInternalFieldCount(
      BaseObject::kInternalFieldCount);

  Local<String> name = OneByteString(isolate, "ModuleWrap");
  tpl->SetClassName(name);

  auto prototype_template = tpl->PrototypeTemplate();
  immortal->SetFunctionProperty(prototype_template, "link", ModuleWrap::Link);
  immortal->SetFunctionProperty(
      prototype_template, "instantiate", ModuleWrap::Instantiate);
  immortal->SetFunctionProperty(
      prototype_template, "getNamespace", ModuleWrap::GetNamespace);
  exports->Set(context, name, tpl->GetFunction(context).ToLocalChecked())
      .Check();

  immortal->SetFunctionProperty(exports,
                                "setImportModuleDynamicallyCallback",
                                ModuleWrap::SetImportModuleDynamicallyCallback);
  immortal->SetFunctionProperty(
      exports,
      "setInitializeImportMetaObjectCallback",
      ModuleWrap::SetInitializeImportMetaObjectCallback);
}

void ModuleWrap::Init(ExternalReferenceRegistry* registry) {
  registry->Register(ModuleWrap::New);
  registry->Register(ModuleWrap::Link);
  registry->Register(ModuleWrap::Instantiate);
  registry->Register(ModuleWrap::GetNamespace);
  registry->Register(ModuleWrap::SetImportModuleDynamicallyCallback);
  registry->Register(ModuleWrap::SetInitializeImportMetaObjectCallback);
}

namespace module_wrap {
AWORKER_BINDING(Init) {
  ModuleWrap::Init(immortal, exports);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  ModuleWrap::Init(registry);
}

}  // namespace module_wrap
}  // namespace aworker

AWORKER_BINDING_REGISTER(module_wrap,
                         aworker::module_wrap::Init,
                         aworker::module_wrap::Init)
