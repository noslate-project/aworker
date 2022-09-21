#ifndef SRC_BINDING_INTERNAL_AWORKER_CACHE_H_
#define SRC_BINDING_INTERNAL_AWORKER_CACHE_H_
#include <string.h>
#include <functional>
#include <map>
#include <vector>

#include "async_wrap.h"
#include "immortal.h"
// Ignore warnings: 'OSAtomicCompareAndSwap32' is deprecated: first deprecated
// in macOS 10.12
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "proto/aworker_cache.pb.h"
#pragma GCC diagnostic pop
#include "util.h"

namespace aworker {
namespace cache {

struct AsyncWorkResult {
  bool success;
  std::string message;
};

template <typename T>
using Callback = std::function<void(AsyncWorkResult, T)>;

template <typename T>
struct CallbackWrap {
  explicit CallbackWrap(T callback) : callback(callback) {}
  T callback;
};

class CacheStorage : public AsyncWrap {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CacheStorage(Immortal* immortal, v8::Local<v8::Object> object);
  // Async.
  void List(Callback<v8::Local<v8::Array>> callback);
  // Async.
  void Has(std::string cacheName, Callback<bool> callback);
  // Async.
  void Ensure(std::string cacheName, Callback<bool> callback);
  // Async.
  void Delete(std::string cacheName, Callback<bool> callback);

  void ReadCachePage(std::string cacheName,
                     Callback<std::shared_ptr<CachePage>> callback);
  void WriteCachePage(std::string cacheName,
                      std::shared_ptr<CachePage> page,
                      Callback<bool> callback);
  void ReadCacheObjectPage(std::string cacheName,
                           std::string cacheObjectFilename,
                           Callback<std::shared_ptr<CacheObjectPage>> callback);
  void WriteCacheObjectPage(std::string cacheName,
                            std::string cacheObjectFilename,
                            std::shared_ptr<CacheObjectPage> page,
                            Callback<bool> callback);

  static v8::Local<v8::Object> New(Immortal* immortal);
  static std::string PathForCacheObjectPage(Immortal* immortal,
                                            std::string cacheName,
                                            const CachedRequest request);
  static std::string PathForCacheObjectPage(Immortal* immortal,
                                            std::string cacheName,
                                            CachedEntry* request);

 private:
  static std::string PathForCacheStorage(Immortal* immortal);
  static std::string PathForCacheStoragePage(Immortal* immortal);
  static std::string PathForCache(Immortal* immortal, std::string cacheName);
  static std::string PathForCachePage(Immortal* immortal,
                                      std::string cacheName);
  static std::string PathForCacheObjectPage(Immortal* immortal,
                                            std::string cacheName,
                                            std::string cacheObjectFilename);
  static void ReadCacheStoragePage(
      Immortal* immortal, Callback<std::shared_ptr<CacheStoragePage>> callback);
  static void WriteCacheStoragePage(Immortal* immortal,
                                    std::shared_ptr<CacheStoragePage> page,
                                    Callback<bool> callback);
  template <typename T>
  static void ReadPage(Immortal* immortal,
                       std::string path,
                       Callback<std::shared_ptr<T>>);
  template <typename T>
  static void WritePage(Immortal* immortal,
                        std::string path,
                        std::shared_ptr<T> page,
                        Callback<bool>);
};

}  // namespace cache
}  // namespace aworker

#endif  // SRC_BINDING_INTERNAL_AWORKER_CACHE_H_
