#ifndef SRC_MEMORY_TRACKER_INL_H_
#define SRC_MEMORY_TRACKER_INL_H_

#include "memory_tracker.h"
#include "util.h"

namespace aworker {

MemoryRetainerNode::MemoryRetainerNode(MemoryTracker* tracker,
                                       const MemoryRetainer* retainer)
    : tracker_(tracker), retainer_(retainer) {
  v8::Local<v8::Object> wrapped_object = retainer->WrappedObject();
  if (!wrapped_object.IsEmpty()) {
    wrapper_node_ = tracker_->graph()->V8Node(wrapped_object);
  }
}

const char* MemoryRetainerNode::Name() {
  return retainer_->MemoryInfoName();
}

size_t MemoryRetainerNode::SizeInBytes() {
  return retainer_->SizeInBytes();
}

const char* MemoryRetainerNode::NamePrefix() {
  return "Aworker /";
}

v8::EmbedderGraph::Node* MemoryRetainerNode::WrapperNode() {
  return wrapper_node_;
}

void MemoryTracker::Track(const MemoryRetainer* retainer) {
  TrackField(nullptr, retainer);
}

void MemoryTracker::TrackField(const char* edge_name,
                               const MemoryRetainer* retainer) {
  v8::HandleScope handle_scope(isolate_);
  auto it = seen_.find(retainer);
  if (it != seen_.end()) {
    if (CurrentNode() != nullptr) {
      graph_->AddEdge(CurrentNode(), it->second, edge_name);
    }
    return;  // It has already been tracked, no need to call MemoryInfo again
  }
  MemoryRetainerNode* n = AddNode(retainer);
  if (CurrentNode() != nullptr) {
    graph_->AddEdge(CurrentNode(), n, edge_name);
  }
  node_stack_.push(n);

  retainer->MemoryInfo(this);
  CHECK_EQ(CurrentNode(), n);
  CHECK_NE(n->SizeInBytes(), 0);

  node_stack_.pop();
}

MemoryRetainerNode* MemoryTracker::CurrentNode() const {
  if (node_stack_.empty()) {
    return nullptr;
  }
  return node_stack_.top();
}

MemoryRetainerNode* MemoryTracker::AddNode(const MemoryRetainer* retainer) {
  auto it = seen_.find(retainer);
  if (it != seen_.end()) {
    return it->second;
  }

  MemoryRetainerNode* n = new MemoryRetainerNode(this, retainer);
  graph_->AddNode(std::unique_ptr<v8::EmbedderGraph::Node>(n));
  seen_[retainer] = n;

  return n;
}

}  // namespace aworker

#endif  // SRC_MEMORY_TRACKER_INL_H_
