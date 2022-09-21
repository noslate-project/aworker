#include <city.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "aworker_binding.h"
#include "aworker_cache.h"
#include "aworker_cache_data.h"
#include "command_parser.h"
#include "debug_utils-inl.h"
#include "error_handling.h"
#include "immortal.h"
#include "proto/proto-inl.pb.h"
#include "util.h"
#include "zero_copy_file_stream.h"

namespace aworker {
namespace cache {

using std::shared_ptr;
using std::string;
using std::unique_ptr;
using v8::Array;
using v8::ArrayBuffer;
using v8::Boolean;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Global;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Uint8Array;
using v8::Value;

// snki
#define AWORKER_MAGIC 0x736e6b69
#define CACHE_DATA_VERSION 1
#define CACHE_PAGE_HEADER_SIZE 10

inline std::string CityHash128(std::string str) {
  uint128 hash = ::CityHash128(str.c_str(), str.length());
  char dest[25];  // including last \0
  CHECK_EQ(pathsafe_b64_encode(dest, reinterpret_cast<char*>(&hash), 16), 24);

  return std::string(dest);
}

bool ParseCachePageHeader(UvZeroCopyInputFileStream* stream) {
  CachePageHeader header;
  const void* data;
  int size;
  if (!stream->Next(&data, &size)) {
    return false;
  }
  CHECK_GE(size, CACHE_PAGE_HEADER_SIZE);
  if (!header.ParseFromArray(data, CACHE_PAGE_HEADER_SIZE)) {
    return false;
  }
  if (header.aworker_magic() != AWORKER_MAGIC) {
    return false;
  }
  if (header.cache_data_version() != CACHE_DATA_VERSION) {
    return false;
  }
  stream->BackUp(size - CACHE_PAGE_HEADER_SIZE);
  return true;
}

bool SerializeCachePageHeader(UvZeroCopyOutputFileStream* stream) {
  CachePageHeader header;
  header.set_aworker_magic(AWORKER_MAGIC);
  header.set_cache_data_version(CACHE_DATA_VERSION);
  if (!header.SerializeToZeroCopyStream(stream)) {
    return false;
  }
  return true;
}

const WrapperTypeInfo CacheStorage::wrapper_type_info_{
    "cache_storage",
};

Local<Object> CacheStorage::New(Immortal* immortal) {
  Local<Context> context = immortal->context();
  return AsyncWrap::GetConstructorTemplate(immortal)
      ->GetFunction(context)
      .ToLocalChecked()
      ->NewInstance(context)
      .ToLocalChecked();
}

CacheStorage::CacheStorage(Immortal* immortal, Local<Object> object)
    : AsyncWrap(immortal, object) {}

void CacheStorage::List(Callback<Local<Array>> req) {
  ReadCacheStoragePage(
      immortal(),
      [this, req = std::move(req)](
          AsyncWorkResult result,
          std::shared_ptr<CacheStoragePage> page) mutable {
        Isolate* isolate = immortal()->isolate();
        HandleScope scope(isolate);
        Local<Context> context = immortal()->context();
        if (!result.success) {
          req({true, ""}, Array::New(isolate));
          return;
        }

        Local<Array> js_arr = Array::New(isolate, page->caches_size());
        for (int idx = 0; idx < page->caches_size(); idx++) {
          MaybeLocal<String> name =
              String::NewFromUtf8(isolate, page->caches(idx).c_str());
          if (name.IsEmpty()) {
            // TODO(chengzhong.wcz): Maybe Local;
            req({false, "STRING_TOO_LONG"}, Array::New(isolate));
            return;
          }
          USE(js_arr->Set(context, idx, name.ToLocalChecked()));
        }

        req(result, js_arr);
      });
}

void CacheStorage::Has(std::string cacheName, Callback<bool> req) {
  ReadCacheStoragePage(
      immortal(),
      [cacheName, req = std::move(req)](
          AsyncWorkResult res, std::shared_ptr<CacheStoragePage> page) mutable {
        if (!res.success) {
          req({true, ""}, false);
          return;
        }
        bool found = false;
        for (int idx = 0; idx < page->caches_size(); idx++) {
          if (page->caches(idx) == cacheName) {
            found = true;
            break;
          }
        }
        req({true, ""}, found);
      });
}

void CacheStorage::Ensure(std::string cacheName, Callback<bool> req) {
  mkdir(PathForCache(immortal(), cacheName).c_str(), 0777);
  ReadCacheStoragePage(
      immortal(),
      [this, cacheName, req](AsyncWorkResult res,
                             std::shared_ptr<CacheStoragePage> page) mutable {
        if (!res.success) {
          page = std::make_shared<CacheStoragePage>();
          page->add_caches(cacheName);
          WriteCacheStoragePage(
              immortal(), page, [req](AsyncWorkResult _, bool __) mutable {
                req({true, ""}, true);
              });
          return;
        }
        bool found = false;
        for (int idx = 0; idx < page->caches_size(); idx++) {
          if (page->caches(idx) == cacheName) {
            found = true;
            break;
          }
        }
        if (!found) {
          page->add_caches(cacheName);
          WriteCacheStoragePage(
              immortal(), page, [req](AsyncWorkResult _, bool __) mutable {
                req({true, ""}, true);
              });
        } else {
          req({true, ""}, true);
        }
      });
}

void CacheStorage::Delete(std::string cacheName, Callback<bool> req) {
  unlink(PathForCachePage(immortal(), cacheName).c_str());
  // TODO(chengzhong.wcz): rmdir
  ReadCacheStoragePage(
      immortal(),
      [this, cacheName, req](AsyncWorkResult res,
                             std::shared_ptr<CacheStoragePage> page) mutable {
        if (!res.success) {
          req(res, false);
          return;
        }
        int32_t delete_count = 0;
        for (int idx = 0; idx < page->caches_size(); idx++) {
          if (page->caches(idx) == cacheName) {
            delete_count++;
            page->mutable_caches()->SwapElements(
                idx, page->caches_size() - delete_count);
            break;
          }
        }
        for (int32_t idx = 0; idx < delete_count; idx++) {
          page->mutable_caches()->RemoveLast();
        }
        if (delete_count > 0) {
          WriteCacheStoragePage(
              immortal(), page, [req](AsyncWorkResult _, bool __) mutable {
                req({true, ""}, true);
              });
        } else {
          req({true, ""}, true);
        }
      });
}

std::string CacheStorage::PathForCacheStorage(Immortal* immortal) {
  return std::string(immortal->commandline_parser()
                         ->raw_same_origin_shared_data_dir()) +
         "/cache-storages";
}

std::string CacheStorage::PathForCacheStoragePage(Immortal* immortal) {
  return PathForCacheStorage(immortal) + ".data";
}

std::string CacheStorage::PathForCache(Immortal* immortal,
                                       std::string cacheName) {
  string data_dir = std::string(
      immortal->commandline_parser()->raw_same_origin_shared_data_dir());
  string cache_name = "cache-" + CityHash128(cacheName);
  return data_dir + "/" + cache_name;
}

std::string CacheStorage::PathForCachePage(Immortal* immortal,
                                           std::string cacheName) {
  return PathForCache(immortal, cacheName) + ".data";
}

std::string CacheStorage::PathForCacheObjectPage(Immortal* immortal,
                                                 std::string cacheName,
                                                 const CachedRequest request) {
  string ctn = request.method() + " " + request.url() + "\n";
  for (auto it = request.headers().begin(); it != request.headers().end();
       it++) {
    ctn += it->key() + ": " + it->value() + "\n";
  }
  return PathForCacheObjectPage(
      immortal, cacheName, CityHash128(ctn) + ".data");
}

std::string CacheStorage::PathForCacheObjectPage(Immortal* immortal,
                                                 std::string cacheName,
                                                 CachedEntry* request) {
  string ctn = request->req_method() + " " + request->url() + "\n";
  for (auto it = request->req_headers().begin();
       it != request->req_headers().end();
       it++) {
    ctn += it->key() + ": " + it->value() + "\n";
  }
  return PathForCacheObjectPage(
      immortal, cacheName, CityHash128(ctn) + ".data");
}

std::string CacheStorage::PathForCacheObjectPage(
    Immortal* immortal,
    std::string cacheName,
    std::string cacheObjectFilename) {
  return PathForCache(immortal, cacheName) + "/" + cacheObjectFilename;
}

void CacheStorage::ReadCacheStoragePage(
    Immortal* immortal, Callback<std::shared_ptr<CacheStoragePage>> req) {
  ReadPage(immortal, PathForCacheStoragePage(immortal), req);
}

void CacheStorage::WriteCacheStoragePage(Immortal* immortal,
                                         std::shared_ptr<CacheStoragePage> page,
                                         Callback<bool> req) {
  WritePage(immortal, PathForCacheStoragePage(immortal), page, req);
}

void CacheStorage::ReadCachePage(std::string cacheName,
                                 Callback<std::shared_ptr<CachePage>> req) {
  ReadPage(immortal(), PathForCachePage(immortal(), cacheName), req);
}

void CacheStorage::WriteCachePage(std::string cacheName,
                                  std::shared_ptr<CachePage> page,
                                  Callback<bool> req) {
  WritePage(immortal(), PathForCachePage(immortal(), cacheName), page, req);
}

void CacheStorage::ReadCacheObjectPage(
    std::string cacheName,
    std::string cacheObjectFilename,
    Callback<std::shared_ptr<CacheObjectPage>> req) {
  ReadPage(immortal(), cacheObjectFilename, req);
}

void CacheStorage::WriteCacheObjectPage(std::string cacheName,
                                        std::string cacheObjectFilename,
                                        std::shared_ptr<CacheObjectPage> page,
                                        Callback<bool> req) {
  WritePage(immortal(), cacheObjectFilename, page, req);
}

template <typename T>
void CacheStorage::ReadPage(Immortal* immortal,
                            std::string path,
                            Callback<std::shared_ptr<T>> req) {
  CallbackWrap<decltype(req)>* req_wrap =
      new CallbackWrap<decltype(req)>(std::move(req));
  UvZeroCopyInputFileStream::Create(
      immortal->event_loop(),
      path,
      [req_wrap_captured = req_wrap,
       path](std::unique_ptr<UvZeroCopyInputFileStream> stream) {
        // To make asan happy that we are not accessing the captured variable
        // after the lambda got released.
        CallbackWrap<decltype(req)>* req_wrap = req_wrap_captured;
        if (stream == nullptr) {
          req_wrap->callback({false, SPrintF("file not found(%s)", path)},
                             nullptr);
          delete req_wrap;
          return;
        }
        if (!ParseCachePageHeader(stream.get())) {
          req_wrap->callback(
              {false, SPrintF("unable to parse page header(%s)", path)},
              nullptr);
          delete req_wrap;
          return;
        }
        UvZeroCopyInputFileStream* stream_managed = stream.release();
        shared_ptr<T> page =
            std::shared_ptr<T>(new T(), [stream_managed](T* p) {
              delete p;
              delete stream_managed;
            });
        page->ParseFromZeroCopyStream(stream_managed);
        req_wrap->callback({true, ""}, std::move(page));
        delete req_wrap;
      });
}

template <typename T>
void CacheStorage::WritePage(Immortal* immortal,
                             std::string path,
                             std::shared_ptr<T> page,
                             Callback<bool> req) {
  CallbackWrap<decltype(req)>* req_wrap =
      new CallbackWrap<decltype(req)>(std::move(req));
  UvZeroCopyOutputFileStream::UvZeroCopyOutputFileStreamPtr stream =
      UvZeroCopyOutputFileStream::Create(
          immortal->event_loop(), path, [req_wrap](auto it) mutable {
            req_wrap->callback({true, ""}, true);
            delete req_wrap;
          });
  CHECK_NE(stream, nullptr);
  SerializeCachePageHeader(stream.get());
  page->SerializeToZeroCopyStream(stream.get());
}

AWORKER_METHOD(ListCacheStorage) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  Global<Function>* callback =
      new Global<Function>(isolate, info[0].As<Function>());

  immortal->cache_storage()->List(
      [immortal, callback](AsyncWorkResult result, Local<Array> cache_names) {
        Isolate* isolate = immortal->isolate();
        HandleScope scope(isolate);

        Local<Function> local_callback = callback->Get(isolate);
        Local<Value> argv[1] = {cache_names};
        // TODO(chengzhong.wcz): proper wrap;
        immortal->cache_storage()->MakeCallback(local_callback, 1, argv);
        delete callback;
      });
}

AWORKER_METHOD(DetectCacheStorage) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  Local<String> cache_name = info[0].As<String>();
  Global<Function>* callback =
      new Global<Function>(isolate, info[1].As<Function>());
  aworker::Utf8Value cache_name_utf8(isolate, cache_name);

  immortal->cache_storage()->Has(
      cache_name_utf8.ToString(),
      [immortal, callback](AsyncWorkResult result, bool exists) {
        Isolate* isolate = immortal->isolate();
        HandleScope scope(isolate);

        Local<Function> local_callback = callback->Get(isolate);
        Local<Value> argv[1] = {Boolean::New(isolate, exists)};
        // TODO(chengzhong.wcz): proper wrap;
        immortal->cache_storage()->MakeCallback(local_callback, 1, argv);
        delete callback;
      });
}

AWORKER_METHOD(EnsureCacheStorage) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  Local<String> cache_name = info[0].As<String>();
  Global<Function>* callback =
      new Global<Function>(isolate, info[1].As<Function>());
  aworker::Utf8Value cache_name_utf8(isolate, cache_name);
  immortal->cache_storage()->Ensure(
      cache_name_utf8.ToString(),
      [immortal, callback](AsyncWorkResult result, bool _) {
        Isolate* isolate = immortal->isolate();
        HandleScope scope(isolate);

        Local<Function> local_callback = callback->Get(isolate);
        Local<Value> argv[1] = {Boolean::New(isolate, _)};
        // TODO(chengzhong.wcz): proper wrap;
        immortal->cache_storage()->MakeCallback(local_callback, 1, argv);
        delete callback;
      });
}

AWORKER_METHOD(DeleteCacheStorage) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  Local<String> cache_name = info[0].As<String>();
  Global<Function>* callback =
      new Global<Function>(isolate, info[1].As<Function>());

  aworker::Utf8Value cache_name_utf8(isolate, cache_name);
  immortal->cache_storage()->Delete(
      cache_name_utf8.ToString(),
      [immortal, callback](AsyncWorkResult result, bool _) {
        Isolate* isolate = immortal->isolate();
        HandleScope scope(isolate);

        Local<Function> local_callback = callback->Get(isolate);
        Local<Value> argv[1] = {Boolean::New(isolate, _)};
        // TODO(chengzhong.wcz): proper wrap;
        immortal->cache_storage()->MakeCallback(local_callback, 1, argv);
        delete callback;
      });
}

AWORKER_METHOD(ReadCachePage) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  Local<String> cache_name = info[0].As<String>();
  Global<Function>* callback =
      new Global<Function>(isolate, info[1].As<Function>());

  aworker::Utf8Value cache_name_utf8(isolate, cache_name);
  immortal->cache_storage()->ReadCachePage(
      *cache_name_utf8,
      [immortal, callback](AsyncWorkResult result,
                           std::shared_ptr<CachePage> page) {
        Isolate* isolate = immortal->isolate();
        HandleScope scope(isolate);
        Local<Context> context = immortal->context();
        // TODO(chengzhong.wcz): error handling
        Local<Function> local_callback = callback->Get(isolate);
        delete callback;

        if (!result.success || page == nullptr) {
          Local<Value> argv[1] = {v8::Exception::TypeError(OneByteString(
              isolate,
              SPrintF("unable to read cache page: %s", result.message)))};
          // TODO(chengzhong.wcz): proper wrap;
          immortal->cache_storage()->MakeCallback(local_callback, 1, argv);
          return;
        }

        Local<Array> js_arr = Array::New(isolate, page->entries_size());
        for (int idx = 0; idx < page->entries_size(); idx++) {
          const CachedEntry& entry = page->entries(idx);
          Local<Value> item = CachedEntryHelper::ToValue(&entry, context);

          USE(js_arr->Set(context, idx, item));
        }

        Local<Value> argv[2] = {v8::Null(isolate), js_arr};
        // TODO(chengzhong.wcz): proper wrap;
        immortal->cache_storage()->MakeCallback(local_callback, 2, argv);
      });
}

AWORKER_METHOD(WriteCachePage) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal->context();

  Local<String> cache_name = info[0].As<String>();
  Local<Array> cache_page = info[1].As<Array>();
  Global<Function>* callback =
      new Global<Function>(isolate, info[2].As<Function>());

  aworker::Utf8Value cache_name_utf8(isolate, cache_name);
  shared_ptr<CachePage> page = std::make_shared<CachePage>();
  page->set_name(*cache_name_utf8);

  for (uint32_t idx = 0; idx < cache_page->Length(); idx++) {
    CachedEntry* item = page->add_entries();
    Local<Object> jval =
        cache_page->Get(context, idx).ToLocalChecked().As<Object>();
    CachedEntryHelper::FromValue(context, jval, item);
    item->set_cache_object_filename(
        CacheStorage::PathForCacheObjectPage(immortal, *cache_name_utf8, item));
  }

  immortal->cache_storage()->WriteCachePage(
      *cache_name_utf8,
      page,
      [immortal, callback](AsyncWorkResult result, bool success) {
        Isolate* isolate = immortal->isolate();
        HandleScope scope(isolate);

        // TODO(chengzhong.wcz): error handling
        Local<Function> local_callback = callback->Get(isolate);
        Local<Value> argv[1] = {Boolean::New(isolate, success)};
        // TODO(chengzhong.wcz): proper wrap;
        immortal->cache_storage()->MakeCallback(local_callback, 1, argv);
        delete callback;
      });
}

AWORKER_METHOD(ReadCacheObjectPage) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);

  Local<String> cache_name = info[0].As<String>();
  Local<String> cache_object_filename = info[1].As<String>();
  Global<Function>* callback =
      new Global<Function>(isolate, info[2].As<Function>());

  aworker::Utf8Value cache_name_utf8(isolate, cache_name);
  aworker::Utf8Value cache_object_filename_utf8(isolate, cache_object_filename);
  immortal->cache_storage()->ReadCacheObjectPage(
      *cache_name_utf8,
      *cache_object_filename_utf8,
      [immortal, callback](AsyncWorkResult result,
                           std::shared_ptr<CacheObjectPage> page) {
        Isolate* isolate = immortal->isolate();
        HandleScope scope(isolate);
        Local<Context> context = immortal->context();
        Local<Function> local_callback = callback->Get(isolate);
        delete callback;

        if (!result.success || page == nullptr) {
          Local<Value> argv[1] = {v8::Exception::TypeError(
              OneByteString(isolate,
                            SPrintF("unable to read cache object page: %s",
                                    result.message)))};
          // TODO(chengzhong.wcz): proper wrap;
          immortal->cache_storage()->MakeCallback(local_callback, 1, argv);
          return;
        }

        CHECK(page->has_request() && page->has_response());
        Local<Value> js_page =
            aworker::cache::CacheObjectPageHelper::ToValue(page.get(), context);
        Local<Value> argv[2] = {v8::Null(isolate), js_page};
        // TODO(chengzhong.wcz): proper wrap;
        immortal->cache_storage()->MakeCallback(local_callback, 2, argv);
      });
}

AWORKER_METHOD(WriteCacheObjectPage) {
  Immortal* immortal = Immortal::GetCurrent(info);
  Isolate* isolate = immortal->isolate();
  HandleScope scope(isolate);
  Local<Context> context = immortal->context();

  Local<String> cache_name = info[0].As<String>();
  Local<Object> cached_request = info[1].As<Object>();
  Local<Object> cached_response = info[2].As<Object>();
  Global<Function>* callback =
      new Global<Function>(isolate, info[3].As<Function>());

  aworker::Utf8Value cache_name_utf8(isolate, cache_name);
  shared_ptr<CacheObjectPage> page = std::make_shared<CacheObjectPage>();

  {
    CachedRequest* item = page->mutable_request();
    aworker::cache::CachedRequestHelper::FromValue(
        context, cached_request, item);
  }
  {
    CachedResponse* item = page->mutable_response();
    aworker::cache::CachedResponseHelper::FromValue(
        context, cached_response, item);
  }

  immortal->cache_storage()->WriteCacheObjectPage(
      *cache_name_utf8,
      CacheStorage::PathForCacheObjectPage(
          immortal, *cache_name_utf8, page->request()),
      page,
      [immortal, callback](AsyncWorkResult result, bool success) {
        Isolate* isolate = immortal->isolate();
        HandleScope scope(isolate);

        // TODO(chengzhong.wcz): error handling
        Local<Function> local_callback = callback->Get(isolate);
        Local<Value> argv[1] = {Boolean::New(isolate, success)};
        // TODO(chengzhong.wcz): proper wrap;
        immortal->cache_storage()->MakeCallback(local_callback, 1, argv);
        delete callback;
      });
}

AWORKER_METHOD(InitCacheStorage) {
  Immortal* immortal = Immortal::GetCurrent(info);
  // TODO(chengzhong.wcz): put the async wrap reference to JavaScript World.
  immortal->set_cache_storage(
      new CacheStorage(immortal, CacheStorage::New(immortal)));
}

AWORKER_BINDING(Init) {
  immortal->SetFunctionProperty(exports, "init", InitCacheStorage);
  immortal->SetFunctionProperty(exports, "listCacheStorage", ListCacheStorage);
  immortal->SetFunctionProperty(
      exports, "detectCacheStorage", DetectCacheStorage);
  immortal->SetFunctionProperty(
      exports, "ensureCacheStorage", EnsureCacheStorage);
  immortal->SetFunctionProperty(
      exports, "deleteCacheStorage", DeleteCacheStorage);

  immortal->SetFunctionProperty(exports, "readCachePage", ReadCachePage);
  immortal->SetFunctionProperty(exports, "writeCachePage", WriteCachePage);
  immortal->SetFunctionProperty(
      exports, "readCacheObjectPage", ReadCacheObjectPage);
  immortal->SetFunctionProperty(
      exports, "writeCacheObjectPage", WriteCacheObjectPage);
}

AWORKER_EXTERNAL_REFERENCE(Init) {
  registry->Register(InitCacheStorage);
  registry->Register(ListCacheStorage);
  registry->Register(DetectCacheStorage);
  registry->Register(EnsureCacheStorage);
  registry->Register(DeleteCacheStorage);
  registry->Register(ReadCachePage);
  registry->Register(WriteCachePage);
  registry->Register(ReadCacheObjectPage);
  registry->Register(WriteCacheObjectPage);
}

}  // namespace cache
}  // namespace aworker

AWORKER_BINDING_REGISTER(cache, aworker::cache::Init, aworker::cache::Init)
