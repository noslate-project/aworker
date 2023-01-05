#ifndef SRC_MEMORY_TRACKER_H_
#define SRC_MEMORY_TRACKER_H_

#include <list>
#include <stack>
#include <string>
#include <unordered_map>

#include "v8-profiler.h"
#include "v8.h"

namespace aworker {

class MemoryTracker;

// Define the node name of a MemoryRetainer to klass
#define MEMORY_INFO_NAME(Klass)                                                \
  inline const char* MemoryInfoName() const override { return #Klass; }

// Set the self size of a MemoryRetainer to the stack-allocated size of a
// certain class
#define SIZE_IN_BYTES(Klass)                                                   \
  inline size_t SizeInBytes() const override { return sizeof(Klass); }

// Used when there is no additional fields to track
#define SET_NO_MEMORY_INFO()                                                   \
  inline void MemoryInfo(aworker::MemoryTracker* tracker) const override {}

class MemoryRetainer {
 public:
  virtual ~MemoryRetainer() = default;

  // Subclasses should implement these methods to provide information
  // for the V8 heap snapshot generator.
  // The MemoryInfo() method is assumed to be called within a context
  // where all the edges start from the node of the current retainer,
  // and point to the nodes as specified by tracker->Track* calls.
  virtual void MemoryInfo(MemoryTracker* tracker) const = 0;
  virtual const char* MemoryInfoName() const = 0;
  virtual size_t SizeInBytes() const = 0;

  virtual v8::Local<v8::Object> WrappedObject() const {
    return v8::Local<v8::Object>();
  }

  virtual bool IsRootNode() const { return false; }
  virtual v8::EmbedderGraph::Node::Detachedness GetDetachedness() const {
    return v8::EmbedderGraph::Node::Detachedness::kUnknown;
  }
};

class MemoryRetainerNode : public v8::EmbedderGraph::Node {
 public:
  inline MemoryRetainerNode(MemoryTracker* tracker,
                            const MemoryRetainer* retainer);

  inline const char* Name() override;
  inline size_t SizeInBytes() override;
  inline const char* NamePrefix() override;
  inline Node* WrapperNode() override;

 private:
  MemoryTracker* tracker_;
  const MemoryRetainer* retainer_;
  Node* wrapper_node_ = nullptr;
};

class MemoryTracker {
 public:
  inline void Track(const MemoryRetainer* retainer);
  inline void TrackField(const char* edge_name, const MemoryRetainer* retainer);

  inline MemoryTracker(v8::Isolate* isolate, v8::EmbedderGraph* graph)
      : isolate_(isolate), graph_(graph) {}
  inline v8::Isolate* isolate() const { return isolate_; }
  inline v8::EmbedderGraph* graph() const { return graph_; }

 private:
  inline MemoryRetainerNode* CurrentNode() const;
  inline MemoryRetainerNode* AddNode(const MemoryRetainer* retainer);

  v8::Isolate* isolate_;
  v8::EmbedderGraph* graph_;
  std::unordered_map<const MemoryRetainer*, MemoryRetainerNode*> seen_;
  std::stack<MemoryRetainerNode*> node_stack_;
};

}  // namespace aworker

#include "memory_tracker-inl.h"

#endif  // SRC_MEMORY_TRACKER_H_
