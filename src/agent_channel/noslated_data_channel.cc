#include "agent_channel/noslated_data_channel.h"
#include <unistd.h>
#include <string>
#include "aworker_logger.h"
#include "command_parser.h"
#include "debug_utils.h"
#include "error_handling.h"
#include "task_queue.h"
#include "util.h"

namespace aworker {
namespace agent {

using namespace ::aworker::ipc;  // NOLINT(build/namespaces)
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;
using v8::Array;
using v8::ArrayBuffer;
using v8::Boolean;
using v8::Context;
using v8::Function;
using v8::Global;
using v8::HandleScope;
using v8::HeapStatistics;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Number;
using v8::Object;
using v8::String;
using v8::TryCatch;
using v8::Uint8Array;
using v8::Value;

NoslatedDataChannel::NoslatedDataChannel(Immortal* immortal,
                                         std::string server_path,
                                         std::string credential,
                                         bool refed)
    : AgentDataChannel(immortal, credential, refed),
      NoslatedService(),
      loop_(immortal_->event_loop()) {
  auto loop_handle = std::make_shared<UvLoop>(loop_);
  delegate_ = std::make_shared<ClientDelegate>(unowned_ptr(this), loop_handle);
  UvSocketHolder::Connect(
      loop_handle,
      server_path,
      delegate_,
      std::bind(&NoslatedDataChannel::OnConnect, this, std::placeholders::_1));
}

NoslatedDataChannel::~NoslatedDataChannel() {
  socket_.reset();
  while (uv_loop_alive(loop_) != 0 && !closed_) {
    uv_run(loop_, UV_RUN_ONCE);
  }
}

void NoslatedDataChannel::Callback(const uint32_t id,
                                   const Local<Value> exception,
                                   const Local<Value> params) {
  per_process::Debug(DebugCategory::AGENT_CHANNEL, "unref %zu\n", ref_count_);
  ref_count_--;
  if (ref_count_ == 0 && socket_ != nullptr) {
    socket_->Unref();
  }

  // TODO(chengzhong.wcz): CallbackScope
  Isolate* isolate = immortal_->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal_->context();

  Local<Function> callback = immortal_->agent_channel_callback();
  Local<Number> v8_id = Number::New(isolate, id);
  Local<Value> argv[] = {v8_id, exception, params};
  auto ret = callback->Call(context, v8::Undefined(isolate), 3, argv);
  CHECK(!ret.IsEmpty());

  task_queue::TickTaskQueue(immortal_);
}

void NoslatedDataChannel::Emit(const uint32_t id,
                               const std::string& event,
                               const Local<Value> params) {
  per_process::Debug(DebugCategory::AGENT_CHANNEL, "ref %zu\n", ref_count_);
  if (ref_count_ == 0) {
    socket_->Ref();
  }
  ref_count_++;

  Isolate* isolate = immortal_->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal_->context();

  Local<Number> local_id = Number::New(isolate, id);
  Local<String> local_event_name =
      String::NewFromUtf8(isolate, event.c_str()).ToLocalChecked();
  Local<Function> handler = immortal_->agent_channel_handler();

  // TODO(chengzhong.wcz): CallbackScope
  Local<Value> argv[] = {local_id, local_event_name, params};
  USE(handler->Call(context, v8::Undefined(isolate), 3, argv));

  task_queue::TickTaskQueue(immortal_);
}

bool NoslatedDataChannel::Feedback(const uint32_t id,
                                   const int32_t code,
                                   const Local<Object> params) {
  per_process::Debug(
      DebugCategory::AGENT_CHANNEL, "feedback %u %d\n", id, code);
  CHECK(CanonicalCode_IsValid(code));
  if (callbacks_.find(id) == callbacks_.end()) {
    return false;
  }

  per_process::Debug(DebugCategory::AGENT_CHANNEL, "unref %zu\n", ref_count_);
  ref_count_--;
  if (ref_count_ == 0) {
    socket_->Unref();
  }

  auto closure = callbacks_[id];
  callbacks_.erase(id);
  closure(code, params);
  return true;
}

void NoslatedDataChannel::Ref() {
  per_process::Debug(DebugCategory::AGENT_CHANNEL, "ref %zu\n", ref_count_);
  if (ref_count_ == 0) {
    socket_->Ref();
  }
  ref_count_++;
}

void NoslatedDataChannel::Unref() {
  per_process::Debug(DebugCategory::AGENT_CHANNEL, "unref %zu\n", ref_count_);
  ref_count_--;
  if (ref_count_ == 0) {
    socket_->Unref();
  }
}

void NoslatedDataChannel::Trigger(unique_ptr<RpcController> controller,
                                  unique_ptr<TriggerRequestMessage> req,
                                  unique_ptr<TriggerResponseMessage> res,
                                  Closure<TriggerResponseMessage> closure) {
  per_process::Debug(DebugCategory::AGENT_CHANNEL,
                     "on trigger(%d)\n",
                     controller->request_id());
  Isolate* isolate = immortal_->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal_->context();
  Context::Scope context_scope(context);

  callbacks_[controller->request_id()] = [this, closure](
                                             int32_t code,
                                             const Local<Object> params) {
    auto isolate = immortal_->isolate();
    HandleScope scope(isolate);
    auto context = immortal_->context();
    if (code != CanonicalCode::OK) {
      auto res = make_unique<ErrorResponseMessage>();
      Local<String> key_message = OneByteString(isolate, "message");
      Local<String> key_stack = OneByteString(isolate, "stack");
      Local<String> message =
          params->Get(context, key_message).ToLocalChecked().As<String>();
      String::Utf8Value message_utf8(isolate, message);
      Local<String> stack =
          params->Get(context, key_stack).ToLocalChecked().As<String>();
      String::Utf8Value stack_utf8(isolate, stack);

      res->set_message(*message_utf8);
      res->set_stack(*stack_utf8);
      closure(static_cast<CanonicalCode>(code), std::move(res), nullptr);
      return;
    }
    auto res = make_unique<TriggerResponseMessage>();

    Local<String> key_status = OneByteString(isolate, "status");
    Local<String> key_headers = OneByteString(isolate, "headers");

    int32_t status = params->Get(context, key_status)
                         .ToLocalChecked()
                         .As<Number>()
                         ->Int32Value(context)
                         .ToChecked();
    res->set_status(status);
    Local<Array> headers =
        params->Get(context, key_headers).ToLocalChecked().As<Array>();
    auto res_metadata = res->mutable_metadata();
    for (uint32_t idx = 0; idx < headers->Length(); idx++) {
      auto kv = res_metadata->add_headers();
      Local<Array> pair =
          headers->Get(context, idx).ToLocalChecked().As<Array>();
      Local<String> key = pair->Get(context, 0).ToLocalChecked().As<String>();
      String::Utf8Value key_utf8(isolate, key);
      Local<String> val = pair->Get(context, 1).ToLocalChecked().As<String>();
      String::Utf8Value val_utf8(isolate, val);

      kv->set_key(*key_utf8);
      kv->set_value(*val_utf8);
    }

    closure(static_cast<CanonicalCode>(code), nullptr, std::move(res));
  };
  Local<String> key_method = OneByteString(isolate, "method");
  Local<String> key_metadata = OneByteString(isolate, "metadata");
  Local<String> key_url = OneByteString(isolate, "url");
  Local<String> key_headers = OneByteString(isolate, "headers");
  Local<String> key_baggage = OneByteString(isolate, "baggage");
  Local<String> key_sid = OneByteString(isolate, "sid");
  Local<String> key_has_input_data = OneByteString(isolate, "hasInputData");
  Local<String> key_has_output_data = OneByteString(isolate, "hasOutputData");

  Local<String> method =
      String::NewFromUtf8(isolate, req->method().c_str()).ToLocalChecked();
  Local<Object> params = Object::New(isolate);
  Local<Object> metadata = Object::New(isolate);
  params->Set(context, key_method, method).Check();
  params->Set(context, key_metadata, metadata).Check();

  params
      ->Set(context,
            key_has_input_data,
            Boolean::New(isolate, req->has_input_data()))
      .Check();
  params
      ->Set(context,
            key_has_output_data,
            Boolean::New(isolate, req->has_output_data()))
      .Check();
  if (req->has_sid()) {
    Local<Number> sid = Number::New(isolate, req->sid());
    params->Set(context, key_sid, sid).Check();
  }

  auto r_metadata = req->metadata();
  if (r_metadata.has_url()) {
    Local<String> url =
        String::NewFromUtf8(isolate, r_metadata.url().c_str()).ToLocalChecked();
    metadata->Set(context, key_url, url).Check();
  }
  if (r_metadata.has_method()) {
    Local<String> method =
        String::NewFromUtf8(isolate, r_metadata.method().c_str())
            .ToLocalChecked();
    metadata->Set(context, key_method, method).Check();
  }
  auto headers = Array::New(isolate, r_metadata.headers_size());
  metadata->Set(context, key_headers, headers).Check();
  for (int idx = 0; idx < r_metadata.headers_size(); idx++) {
    auto kv = r_metadata.headers(idx);
    auto pair = Array::New(isolate, 2);
    Local<String> key =
        String::NewFromUtf8(isolate, kv.key().c_str()).ToLocalChecked();
    Local<String> val =
        String::NewFromUtf8(isolate, kv.value().c_str()).ToLocalChecked();
    pair->Set(context, 0, key).Check();
    pair->Set(context, 1, val).Check();
    headers->Set(context, idx, pair).Check();
  }
  auto baggage = Array::New(isolate, r_metadata.baggage_size());
  metadata->Set(context, key_baggage, baggage).Check();
  for (int idx = 0; idx < r_metadata.baggage_size(); idx++) {
    auto kv = r_metadata.baggage(idx);
    auto pair = Array::New(isolate, 2);
    Local<String> key =
        String::NewFromUtf8(isolate, kv.key().c_str()).ToLocalChecked();
    Local<String> val =
        String::NewFromUtf8(isolate, kv.value().c_str()).ToLocalChecked();
    pair->Set(context, 0, key).Check();
    pair->Set(context, 1, val).Check();
    baggage->Set(context, idx, pair).Check();
  }

  Emit(controller->request_id(), "trigger", params);
}

void NoslatedDataChannel::StreamPush(
    unique_ptr<RpcController> controller,
    unique_ptr<StreamPushRequestMessage> req,
    unique_ptr<StreamPushResponseMessage> res,
    Closure<StreamPushResponseMessage> closure) {
  per_process::Debug(DebugCategory::AGENT_CHANNEL,
                     "on stream push(%d)\n",
                     controller->request_id());
  Isolate* isolate = immortal_->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal_->context();
  Context::Scope context_scope(context);

  callbacks_[controller->request_id()] = [closure](int32_t code,
                                                   const Local<Object> params) {
    auto res = make_unique<StreamPushResponseMessage>();
    // TODO(chengzhong.wcz): serialize errors;
    closure(static_cast<CanonicalCode>(code), nullptr, std::move(res));
  };
  Local<String> key_sid = String::NewFromUtf8(isolate, "sid").ToLocalChecked();
  Local<String> key_is_eos =
      String::NewFromUtf8(isolate, "isEos").ToLocalChecked();
  Local<String> key_is_error =
      String::NewFromUtf8(isolate, "isError").ToLocalChecked();
  Local<String> key_data =
      String::NewFromUtf8(isolate, "data").ToLocalChecked();

  auto req_managed = req.release();
  Local<Number> sid = Number::New(isolate, req_managed->sid());
  Local<Boolean> is_eos = Boolean::New(isolate, req_managed->is_eos());
  Local<Boolean> is_error = Boolean::New(
      isolate, req_managed->has_is_error() ? req_managed->is_error() : false);

  Local<Object> params = Object::New(isolate);
  params->Set(context, key_sid, sid).Check();
  params->Set(context, key_is_eos, is_eos).Check();
  params->Set(context, key_is_error, is_error).Check();

  if (!req_managed->is_eos() && req_managed->has_data()) {
    auto backing_store = ArrayBuffer::NewBackingStore(
        const_cast<char*>(req_managed->data().c_str()),
        req_managed->data().length(),
        [](void* data, size_t length, void* deleter_data) {
          auto req_managed =
              reinterpret_cast<StreamPushRequestMessage*>(deleter_data);
          delete req_managed;
        },
        req_managed);
    auto data = ArrayBuffer::New(isolate, move(backing_store));
    params->Set(context, key_data, data).Check();
  } else {
    delete req_managed;
  }

  Emit(controller->request_id(), "streamPush", params);
}

void NoslatedDataChannel::CollectMetrics(
    unique_ptr<RpcController> controller,
    unique_ptr<CollectMetricsRequestMessage> req,
    unique_ptr<CollectMetricsResponseMessage> res,
    Closure<CollectMetricsResponseMessage> closure) {
  Isolate* isolate = immortal_->isolate();
  HeapStatistics statistics;
  isolate->GetHeapStatistics(&statistics);
#define HEAP_STATISTICS_FIELDS_FOREACH(V)                                      \
  V(total_heap_size)                                                           \
  V(total_heap_size_executable)                                                \
  V(total_physical_size)                                                       \
  V(total_available_size)                                                      \
  V(total_global_handles_size)                                                 \
  V(used_global_handles_size)                                                  \
  V(used_heap_size)                                                            \
  V(heap_size_limit)                                                           \
  V(malloced_memory)                                                           \
  V(external_memory)                                                           \
  V(peak_malloced_memory)                                                      \
  V(number_of_native_contexts)                                                 \
  V(number_of_detached_contexts)

#define V(name)                                                                \
  {                                                                            \
    auto record = res->add_integer_records();                                  \
    record->set_name("noslate.worker." #name);                                 \
    auto label = record->add_labels();                                         \
    label->set_key("noslate.worker.pid");                                      \
    /* TODO(chengzhong.wcz): real pid when pid namespace isolated */           \
    label->set_value(std::to_string(getpid()));                                \
    record->set_value(statistics.name());                                      \
  }
  HEAP_STATISTICS_FIELDS_FOREACH(V)

#undef V
#undef HEAP_STATISTICS_FIELDS_FOREACH

  closure(CanonicalCode::OK, nullptr, move(res));
}

void NoslatedDataChannel::ResourceNotification(
    unique_ptr<RpcController> controller,
    unique_ptr<ResourceNotificationRequestMessage> req,
    unique_ptr<ResourceNotificationResponseMessage> response,
    Closure<ResourceNotificationResponseMessage> closure) {
  per_process::Debug(DebugCategory::AGENT_CHANNEL,
                     "on resource notification(%d)\n",
                     controller->request_id());
  Isolate* isolate = immortal_->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal_->context();
  Context::Scope context_scope(context);

  callbacks_[controller->request_id()] = [closure](int32_t code,
                                                   const Local<Object> params) {
    auto res = make_unique<ResourceNotificationResponseMessage>();
    // TODO(chengzhong.wcz): serialize errors;
    closure(static_cast<CanonicalCode>(code), nullptr, std::move(res));
  };
  Local<String> key_resource_id = OneByteString(isolate, "resourceId");
  Local<String> key_token = OneByteString(isolate, "token");

  Local<String> resource_id =
      String::NewFromUtf8(isolate, req->resource_id().c_str()).ToLocalChecked();
  Local<String> token =
      String::NewFromUtf8(isolate, req->token().c_str()).ToLocalChecked();

  Local<Object> params = Object::New(isolate);
  params->Set(context, key_resource_id, resource_id).Check();
  params->Set(context, key_token, token).Check();

  Emit(controller->request_id(), "resourceNotification", params);
}

void NoslatedDataChannel::CallFetch(unique_ptr<RpcController> controller,
                                    const Local<Object> params) {
  Isolate* isolate = immortal_->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal_->context();

  Local<String> key_url = OneByteString(isolate, "url");
  Local<String> key_method = OneByteString(isolate, "method");
  Local<String> key_sid = OneByteString(isolate, "sid");
  Local<String> key_headers = OneByteString(isolate, "headers");
  Local<String> key_request_id = OneByteString(isolate, "requestId");

  Local<String> url =
      params->Get(context, key_url).ToLocalChecked().As<String>();
  String::Utf8Value url_utf8(isolate, url);
  Local<String> method =
      params->Get(context, key_method).ToLocalChecked().As<String>();
  String::Utf8Value method_utf8(isolate, method);
  Local<Array> headers =
      params->Get(context, key_headers).ToLocalChecked().As<Array>();
  Local<Number> request_id =
      params->Get(context, key_request_id).ToLocalChecked().As<Number>();

  auto req = make_unique<FetchRequestMessage>();
  req->set_url(*url_utf8);
  req->set_method(*method_utf8);
  if (params->Has(context, key_sid).ToChecked()) {
    Local<Number> sid =
        params->Get(context, key_sid).ToLocalChecked().As<Number>();
    req->set_sid(sid->Uint32Value(context).ToChecked());
  }
  req->set_request_id(request_id->Uint32Value(context).ToChecked());
  for (uint32_t i = 0; i < headers->Length(); i += 2) {
    Local<String> header_key =
        headers->Get(context, i).ToLocalChecked().As<String>();
    Local<String> header_value =
        headers->Get(context, i + 1).ToLocalChecked().As<String>();
    String::Utf8Value header_key_utf8(isolate, header_key);
    String::Utf8Value header_value_utf8(isolate, header_value);
    auto pair = req->add_headers();
    pair->set_key(*header_key_utf8);
    pair->set_value(*header_value_utf8);
  }

  auto req_id = controller->request_id();
  Request(
      move(controller),
      move(req),
      [this, req_id](CanonicalCode code,
                     unique_ptr<ErrorResponseMessage> error,
                     unique_ptr<FetchResponseMessage> res) {
        Isolate* isolate = immortal_->isolate();
        HandleScope scope(isolate);
        Local<Context> context = immortal_->context();
        Context::Scope context_scope(context);

        if (code != CanonicalCode::OK) {
          Callback(req_id,
                   ErrorMessageToJsError(code, move(error)),
                   v8::Undefined(isolate));
          return;
        }
        per_process::Debug(DebugCategory::AGENT_CHANNEL,
                           "fetch response: status(%d)\n",
                           res->status());

        Local<Object> params = Object::New(isolate);
        Local<String> key_status = OneByteString(isolate, "status");
        Local<String> key_headers = OneByteString(isolate, "headers");
        Local<String> key_sid = OneByteString(isolate, "sid");

        Local<Number> status = Number::New(isolate, res->status());
        auto res_headers = res->headers();
        Local<Array> headers = Array::New(isolate, res_headers.size() * 2);
        for (int idx = 0; idx < res_headers.size(); idx++) {
          auto pair = res_headers.Get(idx);
          Local<String> key =
              String::NewFromUtf8(isolate, pair.key().c_str()).ToLocalChecked();
          Local<String> value =
              String::NewFromUtf8(isolate, pair.value().c_str())
                  .ToLocalChecked();
          headers->Set(context, idx * 2, key).Check();
          headers->Set(context, idx * 2 + 1, value).Check();
        }
        Local<Number> sid = Number::New(isolate, res->sid());

        params->Set(context, key_status, status).Check();
        params->Set(context, key_headers, headers).Check();
        params->Set(context, key_sid, sid).Check();

        Callback(req_id, v8::Undefined(isolate), params);
      });
}

void NoslatedDataChannel::CallFetchAbort(unique_ptr<RpcController> controller,
                                         const Local<Object> params) {
  Isolate* isolate = immortal_->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal_->context();

  auto req = make_unique<FetchAbortRequestMessage>();
  Local<String> key_request_id = OneByteString(isolate, "requestId");
  Local<Number> request_id =
      params->Get(context, key_request_id).ToLocalChecked().As<Number>();
  req->set_request_id(request_id->Uint32Value(context).ToChecked());

  auto req_id = controller->request_id();
  Request(move(controller),
          move(req),
          [this, req_id](CanonicalCode code,
                         unique_ptr<ErrorResponseMessage> error,
                         unique_ptr<FetchAbortResponseMessage> res) {
            Isolate* isolate = immortal_->isolate();
            HandleScope scope(isolate);
            Local<Context> context = immortal_->context();
            Context::Scope context_scope(context);

            if (code != CanonicalCode::OK) {
              Callback(req_id,
                       ErrorMessageToJsError(code, move(error)),
                       v8::Undefined(isolate));
              return;
            }

            Local<Object> params = Object::New(isolate);
            Callback(req_id, v8::Undefined(isolate), params);
          });
}

void NoslatedDataChannel::CallStreamOpen(unique_ptr<RpcController> controller,
                                         const Local<Object> params) {
  auto req = make_unique<StreamOpenRequestMessage>();

  auto req_id = controller->request_id();
  Request(move(controller),
          move(req),
          [this, req_id](CanonicalCode code,
                         unique_ptr<ErrorResponseMessage> error,
                         unique_ptr<StreamOpenResponseMessage> res) {
            Isolate* isolate = immortal_->isolate();
            HandleScope scope(isolate);
            Local<Context> context = immortal_->context();
            Context::Scope context_scope(context);

            if (code != CanonicalCode::OK) {
              Callback(req_id,
                       ErrorMessageToJsError(code, move(error)),
                       v8::Undefined(isolate));
              return;
            }

            Local<Object> params = Object::New(isolate);
            Local<String> key_sid = OneByteString(isolate, "sid");

            params->Set(context, key_sid, Number::New(isolate, res->sid()))
                .ToChecked();
            Callback(req_id, v8::Undefined(isolate), params);
          });
}

void NoslatedDataChannel::CallStreamPush(unique_ptr<RpcController> controller,
                                         const Local<Object> params) {
  Isolate* isolate = immortal_->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal_->context();

  Local<String> key_sid = OneByteString(isolate, "sid");
  Local<String> key_is_eos = OneByteString(isolate, "isEos");
  Local<String> key_is_error = OneByteString(isolate, "isError");
  Local<String> key_data = OneByteString(isolate, "data");

  Local<Number> sid =
      params->Get(context, key_sid).ToLocalChecked().As<Number>();
  bool is_eos = params->Get(context, key_is_eos)
                    .ToLocalChecked()
                    .As<Boolean>()
                    ->BooleanValue(isolate);
  bool is_error = params->Get(context, key_is_error)
                      .ToLocalChecked()
                      .As<Boolean>()
                      ->BooleanValue(isolate);

  auto req = make_unique<StreamPushRequestMessage>();
  req->set_sid(sid->Uint32Value(context).ToChecked());
  req->set_is_eos(is_eos);
  req->set_is_error(is_error);
  if (!is_eos) {
    Local<Uint8Array> data =
        params->Get(context, key_data).ToLocalChecked().As<Uint8Array>();
    req->set_data(
        static_cast<uint8_t*>(data->Buffer()->GetBackingStore()->Data()) +
            data->ByteOffset(),
        data->ByteLength());
  }

  auto req_id = controller->request_id();
  Request(move(controller),
          move(req),
          [this, req_id](CanonicalCode code,
                         unique_ptr<ErrorResponseMessage> error,
                         unique_ptr<StreamPushResponseMessage> res) {
            Isolate* isolate = immortal_->isolate();
            HandleScope scope(isolate);
            Local<Context> context = immortal_->context();
            Context::Scope context_scope(context);

            if (code != CanonicalCode::OK) {
              Callback(req_id,
                       ErrorMessageToJsError(code, move(error)),
                       v8::Undefined(isolate));
              return;
            }
            Callback(req_id, v8::Undefined(isolate), v8::Undefined(isolate));
          });
}

void NoslatedDataChannel::CallDaprInvoke(unique_ptr<RpcController> controller,
                                         const Local<Object> params) {
  Isolate* isolate = immortal_->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal_->context();

  Local<String> key_app = OneByteString(isolate, "app");
  Local<String> key_method = OneByteString(isolate, "method");
  Local<String> key_body = OneByteString(isolate, "body");

  Local<String> app =
      params->Get(context, key_app).ToLocalChecked().As<String>();
  String::Utf8Value app_utf8(isolate, app);
  Local<String> method =
      params->Get(context, key_method).ToLocalChecked().As<String>();
  String::Utf8Value method_utf8(isolate, method);
  Local<Uint8Array> body =
      params->Get(context, key_body).ToLocalChecked().As<Uint8Array>();

  auto req = make_unique<DaprInvokeRequestMessage>();
  req->set_app_id(*app_utf8);
  req->set_method_name(*method_utf8);
  if (body->HasBuffer()) {
    req->set_data(
        static_cast<uint8_t*>(body->Buffer()->GetBackingStore()->Data()) +
            body->ByteOffset(),
        body->ByteLength());
  } else {
    req->set_data("");
  }

  auto req_id = controller->request_id();
  Request(
      move(controller),
      move(req),
      [this, req_id](CanonicalCode code,
                     unique_ptr<ErrorResponseMessage> error,
                     unique_ptr<DaprInvokeResponseMessage> res) {
        Isolate* isolate = immortal_->isolate();
        HandleScope scope(isolate);
        Local<Context> context = immortal_->context();
        Context::Scope context_scope(context);

        if (code != CanonicalCode::OK) {
          Callback(req_id,
                   ErrorMessageToJsError(code, move(error)),
                   v8::Undefined(isolate));
          return;
        }
        per_process::Debug(DebugCategory::AGENT_CHANNEL,
                           "dapr invoke response: status(%d), body(%s)\n",
                           res->status(),
                           res->data().c_str());

        auto res_managed = res.release();
        Local<Object> params = Object::New(isolate);
        Local<String> key_status = OneByteString(isolate, "status");
        Local<String> key_body = OneByteString(isolate, "body");
        Local<Number> status = Number::New(isolate, res_managed->status());
        auto backing_store = ArrayBuffer::NewBackingStore(
            const_cast<char*>(res_managed->data().c_str()),
            res_managed->data().length(),
            [](void* data, size_t length, void* deleter_data) {
              auto res_managed =
                  reinterpret_cast<DaprInvokeResponseMessage*>(deleter_data);
              delete res_managed;
            },
            res_managed);
        auto body = ArrayBuffer::New(isolate, move(backing_store));
        params->Set(context, key_status, status).Check();
        params->Set(context, key_body, body).Check();

        Callback(req_id, v8::Undefined(isolate), params);
      });
}

void NoslatedDataChannel::CallDaprBinding(unique_ptr<RpcController> controller,
                                          const Local<Object> params) {
  Isolate* isolate = immortal_->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal_->context();

  Local<String> key_name = OneByteString(isolate, "name");
  Local<String> key_metadata = OneByteString(isolate, "metadata");
  Local<String> key_operation = OneByteString(isolate, "operation");
  Local<String> key_body = OneByteString(isolate, "body");

  Local<String> name =
      params->Get(context, key_name).ToLocalChecked().As<String>();
  String::Utf8Value name_utf8(isolate, name);
  Local<String> metadata =
      params->Get(context, key_metadata).ToLocalChecked().As<String>();
  String::Utf8Value metadata_utf8(isolate, metadata);
  Local<String> operation =
      params->Get(context, key_operation).ToLocalChecked().As<String>();
  String::Utf8Value operation_utf8(isolate, operation);
  Local<Uint8Array> body =
      params->Get(context, key_body).ToLocalChecked().As<Uint8Array>();

  auto req = make_unique<DaprBindingRequestMessage>();
  req->set_name(*name_utf8);
  req->set_metadata(*metadata_utf8);
  req->set_operation(*operation_utf8);
  if (body->HasBuffer()) {
    req->set_data(
        static_cast<uint8_t*>(body->Buffer()->GetBackingStore()->Data()) +
            body->ByteOffset(),
        body->ByteLength());
  } else {
    req->set_data("");
  }

  auto req_id = controller->request_id();
  Request(
      move(controller),
      move(req),
      [this, req_id](CanonicalCode code,
                     unique_ptr<ErrorResponseMessage> error,
                     unique_ptr<DaprBindingResponseMessage> res) {
        Isolate* isolate = immortal_->isolate();
        HandleScope scope(isolate);
        Local<Context> context = immortal_->context();
        Context::Scope context_scope(context);

        if (code != CanonicalCode::OK) {
          Callback(req_id,
                   ErrorMessageToJsError(code, move(error)),
                   v8::Undefined(isolate));
          return;
        }
        per_process::Debug(DebugCategory::AGENT_CHANNEL,
                           "dapr binding response: status(%d), body(%s)\n",
                           res->status(),
                           res->data().c_str());

        auto res_managed = res.release();
        Local<Object> params = Object::New(isolate);
        Local<String> key_status = OneByteString(isolate, "status");
        Local<String> key_body = OneByteString(isolate, "body");
        Local<Number> status = Number::New(isolate, res_managed->status());
        auto backing_store = ArrayBuffer::NewBackingStore(
            const_cast<char*>(res_managed->data().c_str()),
            res_managed->data().length(),
            [](void* data, size_t length, void* deleter_data) {
              auto res_managed =
                  reinterpret_cast<DaprInvokeResponseMessage*>(deleter_data);
              delete res_managed;
            },
            res_managed);
        auto body = ArrayBuffer::New(isolate, move(backing_store));
        params->Set(context, key_status, status).Check();
        params->Set(context, key_body, body).Check();

        Callback(req_id, v8::Undefined(isolate), params);
      });
}

void NoslatedDataChannel::CallExtensionBinding(
    unique_ptr<RpcController> controller, const Local<Object> params) {
  Isolate* isolate = immortal_->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal_->context();

  Local<String> key_name = OneByteString(isolate, "name");
  Local<String> key_metadata = OneByteString(isolate, "metadata");
  Local<String> key_operation = OneByteString(isolate, "operation");
  Local<String> key_body = OneByteString(isolate, "body");

  Local<String> name =
      params->Get(context, key_name).ToLocalChecked().As<String>();
  String::Utf8Value name_utf8(isolate, name);
  Local<String> metadata =
      params->Get(context, key_metadata).ToLocalChecked().As<String>();
  String::Utf8Value metadata_utf8(isolate, metadata);
  Local<String> operation =
      params->Get(context, key_operation).ToLocalChecked().As<String>();
  String::Utf8Value operation_utf8(isolate, operation);
  Local<Uint8Array> body =
      params->Get(context, key_body).ToLocalChecked().As<Uint8Array>();

  auto req = make_unique<ExtensionBindingRequestMessage>();
  req->set_name(*name_utf8);
  req->set_metadata(*metadata_utf8);
  req->set_operation(*operation_utf8);
  if (body->HasBuffer()) {
    req->set_data(
        static_cast<uint8_t*>(body->Buffer()->GetBackingStore()->Data()) +
            body->ByteOffset(),
        body->ByteLength());
  } else {
    req->set_data("");
  }

  auto req_id = controller->request_id();
  Request(
      move(controller),
      move(req),
      [this, req_id](CanonicalCode code,
                     unique_ptr<ErrorResponseMessage> error,
                     unique_ptr<ExtensionBindingResponseMessage> res) {
        Isolate* isolate = immortal_->isolate();
        HandleScope scope(isolate);
        Local<Context> context = immortal_->context();
        Context::Scope context_scope(context);

        if (code != CanonicalCode::OK) {
          Callback(req_id,
                   ErrorMessageToJsError(code, move(error)),
                   v8::Undefined(isolate));
          return;
        }
        per_process::Debug(DebugCategory::AGENT_CHANNEL,
                           "dapr binding response: status(%d), body(%s)\n",
                           res->status(),
                           res->data().c_str());

        Local<Object> params = Object::New(isolate);
        Local<String> key_status = OneByteString(isolate, "status");
        Local<String> key_body = OneByteString(isolate, "body");
        Local<Number> status = Number::New(isolate, res->status());
        params->Set(context, key_status, status).Check();
        if (res->has_data()) {
          auto res_managed = res.release();
          auto backing_store = ArrayBuffer::NewBackingStore(
              const_cast<char*>(res_managed->data().c_str()),
              res_managed->data().length(),
              [](void* data, size_t length, void* deleter_data) {
                auto res_managed =
                    reinterpret_cast<DaprInvokeResponseMessage*>(deleter_data);
                delete res_managed;
              },
              res_managed);
          auto body = ArrayBuffer::New(isolate, move(backing_store));
          params->Set(context, key_body, body).Check();
        }

        Callback(req_id, v8::Undefined(isolate), params);
      });
}

void NoslatedDataChannel::CallResourcePut(unique_ptr<RpcController> controller,
                                          const v8::Local<v8::Object> params) {
  Isolate* isolate = immortal_->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal_->context();

  Local<String> key_action = OneByteString(isolate, "action");
  Local<String> key_resource_id = OneByteString(isolate, "resourceId");
  Local<String> key_token = OneByteString(isolate, "token");

  int32_t action = params->Get(context, key_action)
                       .ToLocalChecked()
                       .As<Number>()
                       ->Int32Value(context)
                       .ToChecked();
  CHECK(ResourcePutAction_IsValid(action));
  Local<String> resource_id =
      params->Get(context, key_resource_id).ToLocalChecked().As<String>();
  String::Utf8Value resource_id_utf8(isolate, resource_id);
  Local<String> token =
      params->Get(context, key_token).ToLocalChecked().As<String>();
  String::Utf8Value token_utf8(isolate, token);

  auto req = make_unique<ResourcePutRequestMessage>();
  req->set_action(static_cast<ResourcePutAction>(action));
  req->set_resource_id(*resource_id_utf8);
  req->set_token(*token_utf8);

  auto req_id = controller->request_id();
  Request(
      move(controller),
      move(req),
      [this, req_id](CanonicalCode code,
                     unique_ptr<ErrorResponseMessage> error,
                     unique_ptr<ResourcePutResponseMessage> res) {
        Isolate* isolate = immortal_->isolate();
        HandleScope scope(isolate);
        Local<Context> context = immortal_->context();
        Context::Scope context_scope(context);

        if (code != CanonicalCode::OK) {
          Callback(req_id,
                   ErrorMessageToJsError(code, move(error)),
                   v8::Undefined(isolate));
          return;
        }

        Local<Object> params = Object::New(isolate);
        Local<String> key_success_or_acquired =
            OneByteString(isolate, "success_or_acquired");
        Local<String> key_token = OneByteString(isolate, "token");

        Local<Boolean> success_or_acquired =
            Boolean::New(isolate, res->success_or_acquired());
        Local<String> token =
            String::NewFromUtf8(isolate, res->token().c_str()).ToLocalChecked();
        params->Set(context, key_success_or_acquired, success_or_acquired)
            .Check();
        params->Set(context, key_token, token).Check();

        Callback(req_id, v8::Undefined(isolate), params);
      });
}

void NoslatedDataChannel::OnConnect(UvSocketHolder::Pointer socket) {
  per_process::Debug(DebugCategory::AGENT_CHANNEL, "on connect\n");
  socket_ = std::move(socket);
  if (socket_ != nullptr) {
    per_process::Debug(DebugCategory::AGENT_CHANNEL, "binding credentials\n");
    auto controller = NewControllerWithTimeout(1000);
    auto msg = std::make_unique<CredentialsRequestMessage>();
    msg->set_cred(cred_);
    msg->set_type(CredentialTargetType::Data);
    this->NoslatedService::Request(
        move(controller),
        move(msg),
        [this](CanonicalCode code,
               unique_ptr<ErrorResponseMessage> error,
               unique_ptr<CredentialsResponseMessage> msg) {
          if (code != CanonicalCode::OK) {
            ELOG("Agent rejected credentials with code(%d).", code);
            socket_.reset();
            return;
          }
          // Reference counting of active readers;
          socket_->Unref();
          set_connected();
        });
  }
}

Local<Value> NoslatedDataChannel::ErrorMessageToJsError(
    CanonicalCode code, unique_ptr<ErrorResponseMessage> error) {
  auto isolate = immortal_->isolate();
  auto context = immortal_->context();
  std::string msg = "Noslated request failed with CanonicalCode::";
  std::string peer_message = "";
  std::string peer_stack = "";
#define V(KEY)                                                                 \
  case (CanonicalCode::KEY): {                                                 \
    msg += #KEY;                                                               \
    break;                                                                     \
  }
  switch (code) { NOSLATED_CANONICAL_CODE_KEYS(V) }
#undef V

  if (error != nullptr && error->has_message()) {
    msg += ": " + error->message();
    peer_message = error->message();
  }
  if (error != nullptr && error->has_stack()) {
    peer_stack = error->stack();
  }
  Local<Object> exception =
      v8::Exception::Error(
          String::NewFromUtf8(isolate, msg.c_str()).ToLocalChecked())
          .As<Object>();
  Local<String> code_key = OneByteString(isolate, "code");
  Local<String> peer_message_key = OneByteString(isolate, "peerMessage");
  Local<String> peer_stack_key = OneByteString(isolate, "peerStack");
  exception->Set(context, code_key, Number::New(isolate, code)).Check();
  exception
      ->Set(context,
            peer_message_key,
            String::NewFromUtf8(isolate, peer_message.c_str()).ToLocalChecked())
      .Check();
  exception
      ->Set(context,
            peer_stack_key,
            String::NewFromUtf8(isolate, peer_stack.c_str()).ToLocalChecked())
      .Check();
  return exception;
}

void NoslatedDataChannel::OnError() {
  ELOG("on error");
}

void NoslatedDataChannel::Disconnected(SessionId) {
  per_process::Debug(DebugCategory::AGENT_CHANNEL, "on disconnected\n");
  socket_.reset();
}

void NoslatedDataChannel::Closed() {
  per_process::Debug(DebugCategory::AGENT_CHANNEL, "on closed\n");
  closed_ = true;
}

AWORKER_METHOD(SetCallback) {
  Immortal* immortal = Immortal::GetCurrent(info);
  HandleScope scope(immortal->isolate());

  Local<Function> callback = info[0].As<Function>();
  immortal->set_agent_channel_callback(callback);
}

AWORKER_METHOD(SetHandler) {
  Immortal* immortal = Immortal::GetCurrent(info);
  HandleScope scope(immortal->isolate());

  Local<Function> handler = info[0].As<Function>();
  immortal->set_agent_channel_handler(handler);
}

template <
    void (NoslatedDataChannel::*func)(std::unique_ptr<RpcController> controller,
                                      const v8::Local<v8::Object> params)>
AWORKER_METHOD(NoslatedDataChannel::JsCall) {
  Immortal* immortal = Immortal::GetCurrent(info);

  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal->context();

  if (immortal->agent_data_channel() == nullptr) {
    ThrowException(isolate,
                   "Cannot invoke agent methods when agent is not setup",
                   ExceptionType::kError);
    return;
  }

  std::shared_ptr<NoslatedDataChannel> channel =
      std::static_pointer_cast<NoslatedDataChannel>(
          immortal->agent_data_channel());

  Local<Object> params = info[0].As<Object>();
  uint32_t timeout = info[1].As<Number>()->Uint32Value(context).ToChecked();

  per_process::Debug(
      DebugCategory::AGENT_CHANNEL, "ref %zu\n", channel->ref_count_);
  if (channel->ref_count_ == 0) {
    channel->socket_->Ref();
  }
  channel->ref_count_++;
  auto controller = channel->NewControllerWithTimeout(timeout);
  uint32_t request_id = controller->request_id();
  (channel.get()->*func)(move(controller), params);

  info.GetReturnValue().Set(Number::New(isolate, request_id));
}

AWORKER_METHOD(Feedback) {
  Immortal* immortal = Immortal::GetCurrent(info);

  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal->context();

  if (immortal->agent_data_channel() == nullptr) {
    ThrowException(isolate,
                   "Cannot feedback to agent when agent is not setup",
                   ExceptionType::kError);
    return;
  }

  std::shared_ptr<NoslatedDataChannel> channel =
      std::static_pointer_cast<NoslatedDataChannel>(
          immortal->agent_data_channel());

  Local<Number> id = info[0].As<Number>();
  Local<Number> code = info[1].As<Number>();
  Local<Object> params = info[2].As<Object>();
  if (!channel->Feedback(id->Uint32Value(context).ToChecked(),
                         code->Int32Value(context).ToChecked(),
                         params)) {
    char err[256];
    snprintf(err, sizeof(err) - 1, "Failed to write to agent");

    ThrowException(isolate, err);
    return;
  }
}

AWORKER_METHOD(Ref) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  if (immortal->agent_data_channel() == nullptr) {
    ThrowException(isolate,
                   "Cannot ref agent when agent is not setup",
                   ExceptionType::kError);
    return;
  }

  immortal->agent_data_channel()->Ref();
}

AWORKER_METHOD(Unref) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  if (immortal->agent_data_channel() == nullptr) {
    ThrowException(isolate,
                   "Cannot unref agent when agent is not setup",
                   ExceptionType::kError);
    return;
  }

  immortal->agent_data_channel()->Unref();
}

#define NOSLATED_DATA_CHANNEL_METHODS(V)                                       \
  V(CallFetch, fetch)                                                          \
  V(CallFetchAbort, fetchAbort)                                                \
  V(CallStreamOpen, streamOpen)                                                \
  V(CallStreamPush, streamPush)                                                \
  V(CallDaprInvoke, daprInvoke)                                                \
  V(CallDaprBinding, daprBinding)                                              \
  V(CallExtensionBinding, extensionBinding)                                    \
  V(CallResourcePut, resourcePut)

AWORKER_BINDING(Init) {
#define V(method, name)                                                        \
  immortal->SetFunctionProperty(                                               \
      exports,                                                                 \
      #name,                                                                   \
      NoslatedDataChannel::JsCall<&NoslatedDataChannel::method>);
  NOSLATED_DATA_CHANNEL_METHODS(V)
#undef V

  immortal->SetFunctionProperty(exports, "setCallback", SetCallback);
  immortal->SetFunctionProperty(exports, "setHandler", SetHandler);
  immortal->SetFunctionProperty(exports, "feedback", Feedback);
  immortal->SetFunctionProperty(exports, "ref", Ref);
  immortal->SetFunctionProperty(exports, "unref", Unref);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
#define V(method, name)                                                        \
  registry->Register(NoslatedDataChannel::JsCall<&NoslatedDataChannel::method>);
  NOSLATED_DATA_CHANNEL_METHODS(V)
#undef V

  registry->Register(SetCallback);
  registry->Register(SetHandler);
  registry->Register(Feedback);
  registry->Register(Ref);
  registry->Register(Unref);
}

}  // namespace agent
}  // namespace aworker

AWORKER_BINDING_REGISTER(noslated_data_channel,
                         aworker::agent::Init,
                         aworker::agent::Init)
