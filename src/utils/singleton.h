#ifndef SRC_UTILS_SINGLETON_H_
#define SRC_UTILS_SINGLETON_H_

namespace aworker {

template <class T>
class Singleton {
 public:
  static T& Instance() {
    static T instance;
    return instance;
  }

 protected:
  Singleton() {}
  virtual ~Singleton() {}

 private:
  Singleton(const Singleton&);
  Singleton& operator=(const Singleton&);
};

}  // namespace aworker

#endif  // SRC_UTILS_SINGLETON_H_
