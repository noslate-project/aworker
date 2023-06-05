#include "error_handling.h"
#include "aworker.h"
#include "aworker_binding.h"
#include "command_parser.h"
#include "debug_utils.h"
#include "diag_report.h"
#include "immortal.h"
#include "inspector/inspector_agent.h"
#include "util.h"

namespace aworker {

using std::string;

using v8::Context;
using v8::Exception;
using v8::Function;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Message;
using v8::Number;
using v8::Object;
using v8::Promise;
using v8::PromiseRejectEvent;
using v8::PromiseRejectMessage;
using v8::ScriptOrigin;
using v8::String;
using v8::TryCatch;
using v8::Undefined;
using v8::Value;

TryCatchScope::~TryCatchScope() {
  if (HasCaught() && !HasTerminated() && _mode == CatchMode::kFatal) {
    HandleScope scope(_immortal->isolate());
    Local<v8::Value> exception = Exception();
    Local<v8::Message> message = Message();
    if (message.IsEmpty())
      message = Exception::CreateMessage(_immortal->isolate(), exception);
    FatalException(_immortal->isolate(), exception, message);
  }
}

void PrintCaughtException(v8::Local<v8::Context> context,
                          const v8::TryCatch& try_catch) {
  v8::Isolate* isolate = context->GetIsolate();
  Local<v8::Value> exception = try_catch.Exception();
  PrintCaughtException(context, exception);
}

void PrintCaughtException(v8::Local<v8::Context> context,
                          v8::Local<v8::Value> exception) {
  v8::Isolate* isolate = context->GetIsolate();

  MaybeLocal<Value> message_val;
  MaybeLocal<Value> name_val;

  if (exception->IsObject()) {
    Local<Object> err_obj = exception.As<Object>();
    message_val = err_obj->Get(context, OneByteString(isolate, "message"));
    name_val = err_obj->Get(context, OneByteString(isolate, "name"));
  }

  if (message_val.IsEmpty() || message_val.ToLocalChecked()->IsUndefined() ||
      name_val.IsEmpty() || name_val.ToLocalChecked()->IsUndefined()) {
    // Maybe not an error object. Just print as-is.
    aworker::Utf8Value to_string_utf8(isolate, exception);

    fprintf(stderr,
            "%s\n",
            *to_string_utf8 ? to_string_utf8.ToString().c_str()
                            : "<toString() threw exception>");
  } else {
    aworker::Utf8Value name_utf8(isolate, name_val.ToLocalChecked());
    aworker::Utf8Value message_utf8(isolate, message_val.ToLocalChecked());

    fprintf(stderr, "%s: %s\n", *name_utf8, *message_utf8);
  }
}

void ThrowException(Isolate* isolate, const char* message, ExceptionType type) {
  auto error = GenException(isolate, message, type);
  isolate->ThrowException(error);
}

Local<Value> GenException(Isolate* isolate,
                          const char* message,
                          ExceptionType type) {
  Local<Value> error;
  Local<String> v8_message =
      String::NewFromUtf8(isolate, message).ToLocalChecked();

  switch (type) {
    case ExceptionType::kRangeError:
      error = Exception::RangeError(v8_message);
      break;

    case ExceptionType::kReferenceError:
      error = Exception::ReferenceError(v8_message);
      break;

    case ExceptionType::kSyntaxError:
      error = Exception::SyntaxError(v8_message);
      break;

    case ExceptionType::kTypeError:
      error = Exception::TypeError(v8_message);
      break;

    case ExceptionType::kError:
    default:
      error = Exception::Error(v8_message);
      break;
  }

  return error;
}

static void TryTriggerDiagReport(const char* message,
                                 const char* trigger,
                                 v8::Local<v8::Value> error) {
  // TODO(chengzhong.wcz): check per process command line option to see if
  // report is expected.
  Isolate* isolate = Isolate::TryGetCurrent();
  if (isolate == nullptr) {
    return;
  }
  Immortal* immortal = Immortal::GetCurrent(isolate);
  if (immortal == nullptr) {
    return;
  }
  if (immortal->commandline_parser()->report()) {
    report::TriggerDiagReport(isolate, immortal, message, trigger, error);
  }
}

void FatalError(const char* location, const char* message) {
  if (location) {
    fprintf(stderr, "FATAL ERROR: %s %s\n", location, message);
  } else {
    fprintf(stderr, "FATAL ERROR: %s\n", message);
  }
  fflush(stderr);

  // TODO(chengzhong.wcz): safe Local.
  TryTriggerDiagReport(message, "FatalError", Local<Value>());

  ABORT();
}

void FatalException(Isolate* isolate,
                    Local<Value> exception,
                    Local<Message> message) {
  Immortal* immortal = Immortal::GetCurrent(isolate);
  aworker::Utf8Value msg_utf8(isolate, message->Get());
  fprintf(stderr, "FATAL: %s\n", *msg_utf8);

  PrintCaughtException(immortal->context(), exception);
  fflush(stderr);

  TryTriggerDiagReport(*msg_utf8, "FatalException", exception);

  ABORT();
}

static std::string GetExceptionSource(Isolate* isolate,
                                      Local<Context> context,
                                      Local<Message> message) {
  MaybeLocal<String> source_line_maybe = message->GetSourceLine(context);
  Utf8Value encoded_source(isolate, source_line_maybe.ToLocalChecked());
  std::string sourceline(*encoded_source, encoded_source.length());

  // Print (filename):(line number): (message).
  ScriptOrigin origin = message->GetScriptOrigin();
  Utf8Value filename(isolate, message->GetScriptResourceName());
  const char* filename_string = *filename;
  int linenum = message->GetLineNumber(context).FromJust();

  int script_start =
      (linenum - origin.LineOffset()) == 1 ? origin.ColumnOffset() : 0;
  int start = message->GetStartColumn(context).FromMaybe(0);
  int end = message->GetEndColumn(context).FromMaybe(0);
  if (start >= script_start) {
    CHECK_GE(end, start);
    start -= script_start;
    end -= script_start;
  }

  std::string buf =
      SPrintF("%s:%i\n%s\n", filename_string, linenum, sourceline.c_str());
  CHECK_GT(buf.size(), 0);

  constexpr int kUnderlineBufsize = 1020;
  char underline_buf[kUnderlineBufsize + 4];
  int off = 0;
  // Print wavy underline (GetUnderline is deprecated).
  for (int i = 0; i < start; i++) {
    if (sourceline[i] == '\0' || off >= kUnderlineBufsize) {
      break;
    }
    CHECK_LT(off, kUnderlineBufsize);
    underline_buf[off++] = (sourceline[i] == '\t') ? '\t' : ' ';
  }
  for (int i = start; i < end; i++) {
    if (sourceline[i] == '\0' || off >= kUnderlineBufsize) {
      break;
    }
    CHECK_LT(off, kUnderlineBufsize);
    underline_buf[off++] = '^';
  }
  CHECK_LE(off, kUnderlineBufsize);
  underline_buf[off++] = '\n';

  return buf + std::string(underline_buf, off);
}

static void PrintExceptionSourceLine(Isolate* isolate,
                                     Local<Context> context,
                                     Local<Message> message) {
  std::string error_source = GetExceptionSource(isolate, context, message);
  fprintf(stderr, "%s", error_source.c_str());
}

namespace errors {
// TODO(chengzhong.wcz): message might be empty, fix it
void PrintFatalException(Isolate* isolate,
                         Local<Value> error,
                         Local<Message> message) {
  Immortal* immortal = Immortal::GetCurrent(isolate);
  HandleScope scope(isolate);
  Local<Context> context = immortal->context();

  if (!message.IsEmpty()) {
    PrintExceptionSourceLine(isolate, context, message);
  }

  Local<Value> trace_value;
  if (error->IsNullOrUndefined()) {
    trace_value = Undefined(isolate);
  } else if (!error->IsObject()) {
    error->ToString(context).ToLocal(&trace_value);
  } else {
    auto maybe_trace_value =
        error.As<Object>()->Get(context, immortal->stack_string());
    if (maybe_trace_value.IsEmpty()) {
      trace_value = Undefined(isolate);
    }
  }

  String::Utf8Value trace(isolate, trace_value);
  if (trace.length() > 0 && !trace_value->IsUndefined()) {
    fprintf(stderr, "%s\n", *trace);
  } else {
    // this really only happens for RangeErrors, since they're the only
    // kind that won't have all this info in the trace, or when non-Error
    // objects are thrown manually.
    Local<Value> message;
    Local<Value> name;

    if (error->IsObject()) {
      Local<Object> error_obj = error.As<Object>();

      MaybeLocal<Value> maybe_message =
          error_obj->Get(context, immortal->stack_string());
      CHECK(!maybe_message.IsEmpty());

      MaybeLocal<Value> maybe_name =
          error_obj->Get(context, immortal->name_string());
      CHECK(!maybe_name.IsEmpty());
      message = maybe_message.ToLocalChecked();
      name = maybe_name.ToLocalChecked();
    }

    if (message.IsEmpty() || message->IsUndefined() || name.IsEmpty() ||
        name->IsUndefined()) {
      // Not an error object. Just print as-is.
      String::Utf8Value message(isolate, error);
      fprintf(stderr, "%s\n", *message);
    } else {
      String::Utf8Value message_string(isolate, message);
      fprintf(stderr, "%s\n", *message_string);
    }
  }

  fflush(stderr);
}

void EmitUncaughtException(Isolate* isolate,
                           Local<Value> error,
                           Local<Message> message) {
  Immortal* immortal = Immortal::GetCurrent(isolate);
  CHECK(!error.IsEmpty());
  HandleScope scope(isolate);

  if (message.IsEmpty()) message = Exception::CreateMessage(isolate, error);

  CHECK(isolate->InContext());
  Local<Context> context = immortal->context();

  immortal->inspector_agent()->ReportUncaughtException(error, message);

  // Invoke process.emitUncaughtException() to give user a chance to handle it.
  // We have to grab it from the process object since this has been
  // monkey-patchable.
  Local<Object> process_object = immortal->process_object();
  Local<String> fatal_exception_string =
      immortal->emit_uncaught_exception_string();
  Local<Value> fatal_exception_function =
      process_object->Get(context, fatal_exception_string).ToLocalChecked();
  // If the exception happens before process.emitUncaughtException is attached
  // during bootstrap, or if the user has patched it incorrectly, exit
  // the current Node.js instance.
  if (!fatal_exception_function->IsFunction()) {
    fprintf(stderr, "process.emitUncaughtException is not a function.");
    immortal->Exit(6);
    return;
  }

  MaybeLocal<Value> handled;
  TryCatch try_catch(isolate);

  // Explicitly disable verbose exception reporting -
  // if process._fatalException() throws an error, we don't want it to
  // trigger the per-isolate message listener which will call this
  // function and recurse.
  try_catch.SetVerbose(false);
  Local<Value> argv[1] = {error};
  handled = fatal_exception_function.As<Function>()->Call(
      context, context->Global(), 1, argv);

  if (handled.IsEmpty()) {
    return;
  }

  if (handled.ToLocalChecked()->IsTrue()) {
    return;
  }

  PrintFatalException(isolate, error, message);

  TryTriggerDiagReport("UncaughtException", "UncaughtException", error);

  immortal->Exit(1);
}

void EmitUncaughtException(Isolate* isolate, const TryCatch& try_catch) {
  if (try_catch.IsVerbose()) {
    return;
  }

  CHECK(!try_catch.HasTerminated());
  CHECK(try_catch.HasCaught());
  HandleScope scope(isolate);
  EmitUncaughtException(isolate, try_catch.Exception(), try_catch.Message());
}

void OnMessage(Local<Message> message, Local<Value> error) {
  EmitUncaughtException(Isolate::GetCurrent(), error, message);
}

void OnPromiseReject(PromiseRejectMessage message) {
  Isolate* isolate = Isolate::GetCurrent();
  Immortal* immortal = Immortal::GetCurrent(isolate);

  Local<Number> event = Number::New(isolate, message.GetEvent());
  Local<Promise> promise = message.GetPromise();
  Local<Value> reason = message.GetValue();
  // reason in event PromiseRejectEvent::kPromiseHandlerAddedAfterReject is
  // empty
  if (reason.IsEmpty()) {
    reason = Undefined(isolate);
  }

  CHECK(!promise.IsEmpty());

  HandleScope scope(isolate);
  CHECK(isolate->InContext());
  auto context = immortal->context();

  // Invoke process._emitPromiseRejection() to give user a chance to handle it.
  // We have to grab it from the process object since this has been
  // monkey-patchable.
  Local<Object> process_object = immortal->process_object();
  Local<String> emit_promise_rejection_string =
      immortal->emit_promise_rejection_string();
  Local<Value> emit_promise_rejection_function =
      process_object->Get(context, emit_promise_rejection_string)
          .ToLocalChecked();
  // If the exception happens before process._emitPromiseRejection is attached
  // during bootstrap, or if the user has patched it incorrectly, exit
  // the current Node.js instance.
  CHECK(emit_promise_rejection_function->IsFunction());

  MaybeLocal<Value> handled;
  TryCatch try_catch(isolate);

  // Explicitly disable verbose exception reporting -
  // if process._emitPromiseRejection() throws an error, we don't want it to
  // trigger the per-isolate message listener which will call this
  // function and recurse.
  try_catch.SetVerbose(false);
  Local<Value> argv[3] = {event, promise, reason};
  handled = emit_promise_rejection_function.As<Function>()->Call(
      context, process_object, 3, argv);

  if (try_catch.HasCaught()) {
    PrintFatalException(isolate, try_catch.Exception(), try_catch.Message());
    CHECK(!try_catch.HasCaught());
    UNREACHABLE();
  }

  if (handled.IsEmpty()) {
    return;
  }

  if (handled.ToLocalChecked()->IsTrue()) {
    return;
  }

  // TODO(chengzhong.wcz): safe Local;
  PrintFatalException(isolate, reason, Local<Message>());

  immortal->Exit(1);
}

AWORKER_BINDING(Init) {
#define V(name, value) immortal->SetIntegerProperty(exports, #name, value);
  V(kPromiseRejectWithNoHandler,
    PromiseRejectEvent::kPromiseRejectWithNoHandler)
  V(kPromiseHandlerAddedAfterReject,
    PromiseRejectEvent::kPromiseHandlerAddedAfterReject)
  V(kPromiseRejectAfterResolved,
    PromiseRejectEvent::kPromiseRejectAfterResolved)
  V(kPromiseResolveAfterResolved,
    PromiseRejectEvent::kPromiseResolveAfterResolved)
#undef V
}

AWORKER_EXTERNAL_REFERENCE(Init) {}

}  // namespace errors
}  // namespace aworker

AWORKER_BINDING_REGISTER(errors, aworker::errors::Init, aworker::errors::Init)
