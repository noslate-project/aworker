#ifndef SRC_AWORKER_H_
#define SRC_AWORKER_H_

#include <memory>
#include "aworker_platform.h"
#include "command_parser.h"
#include "immortal.h"
#include "snapshot/snapshot_builder.h"
#include "uv.h"
#include "v8.h"

namespace aworker {

char** InitializeOncePerProcess(int* argc, char** argv);
bool TearDownOncePerProcess();

namespace per_process {
extern bool external_snapshot_loaded;
}  // namespace per_process

enum class IsolateCreationMode {
  kNormal,
  kCreateSnapshot,
  /**
   * Testing mode with no snapshots
   */
  kTesting,
};

class AworkerMainInstance {
  static void DisposeIsolate(v8::Isolate*);
  using IsolatePtr = DeleteFnPtr<v8::Isolate, DisposeIsolate>;

 public:
  static const int kMainContextIndex = 0;

  explicit AworkerMainInstance(std::unique_ptr<CommandlineParserGroup> cli);
  ~AworkerMainInstance();

  /**
   * Initialize with startup options.
   */
  void Initialize(IsolateCreationMode mode = IsolateCreationMode::kNormal);
  /**
   * Evaluate per-execution (non-forkable/non-snapshottable) native modules and
   * load user land codes.
   */
  int Start();

  /**
   * Build snapshot for userland code.
   */
  int BuildSnapshot();

  /**
   * TODO(chengzhong.wcz): Split immortals/environments?
   * Get a non-owner pointer to the immortal.
   */
  inline Immortal* immortal() { return immortal_; }
  /**
   * Take ownership of the Immortal.
   */
  inline std::unique_ptr<Immortal> GetImmortal() {
    return std::move(immortal_owner_);
  }

  /**
   * Get a non-owner pointer to the v8::Isolate.
   */
  inline v8::Isolate* isolate() { return isolate_; }
  /**
   * Take the ownership of the v8::Isolate.
   * Note: AworkerMainInstance will still referencing the isolate.
   */
  inline v8::Isolate* ReleaseIsolate() {
    isolate_owner_->Exit();
    return isolate_owner_.release();
  }

  /**
   * Get a non-owner pointer to the IsolateData.
   */
  inline IsolateData* isolate_data() { return isolate_data_; }
  /**
   * Take the ownership of the IsolateData.
   */
  inline std::unique_ptr<IsolateData> GetIsolateData() {
    return std::move(isolate_data_owner_);
  }

  inline std::shared_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator() {
    return array_buffer_allocator_;
  }

  inline AworkerPlatform* platform() { return platform_.get(); }

 private:
  v8::StartupData* GetSnapshotBlob();
  const std::vector<size_t>* GetIsolateDataIndexes();
  const std::vector<size_t>* GetImmortalDataIndexes();
  const std::vector<NativeModuleCache>* GetNativeModuleCaches();

  void WarmFork();
  void WarmForkWithUserScript();

  // Note: Order matters
  std::unique_ptr<CommandlineParserGroup> cli_;
  uv_loop_t loop_;
  AworkerPlatform::AworkerPlatformPtr platform_;
  std::shared_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator_;
  std::unique_ptr<SnapshotData> snapshot_data_;

  IsolatePtr isolate_owner_;
  std::unique_ptr<IsolateData> isolate_data_owner_;
  std::unique_ptr<Immortal> immortal_owner_;

  v8::Isolate* isolate_;
  IsolateData* isolate_data_;
  Immortal* immortal_;
};

}  // namespace aworker

#endif  // SRC_AWORKER_H_
