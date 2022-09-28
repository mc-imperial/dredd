// Copyright 2022 The Dredd Project Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "libdredd/mutation_tree_node.h"

#include <utility>

namespace dredd {

void MutationTreeNode::AddChild(std::unique_ptr<MutationTreeNode> node) {
  children_.push_back(std::move(node));
}

void MutationTreeNode::AddMutation(std::unique_ptr<Mutation> mutation) {
  mutations_.push_back(std::move(mutation));
}

void MutationTreeNode::TidyUp() {
  PruneEmptySubtrees();
  Compress();
}

bool MutationTreeNode::IsEmpty() const {
  if (!mutations_.empty()) {
    return false;
  }
  for (const auto& child : children_) {
    if (!child->IsEmpty()) {
      return false;
    }
  }
  return true;
}

void MutationTreeNode::Compress() {
  while (mutations_.empty() && children_.size() == 1) {
    std::vector<std::unique_ptr<Mutation>> child_mutations;
    for (auto& child_mutation : children_[0]->mutations_) {
      child_mutations.push_back(std::move(child_mutation));
    }
    std::vector<std::unique_ptr<MutationTreeNode>> child_children;
    for (auto& child_child : children_[0]->children_) {
      child_children.push_back(std::move(child_child));
    }
    mutations_ = std::move(child_mutations);
    children_ = std::move(child_children);
  }
  for (auto& child : children_) {
    child->Compress();
  }
}

void MutationTreeNode::PruneEmptySubtrees() {
  auto child_iterator = children_.begin();
  while (child_iterator != children_.end()) {
    if ((*child_iterator)->IsEmpty()) {
      child_iterator = children_.erase(child_iterator);
    } else {
      (*child_iterator)->PruneEmptySubtrees();
      ++child_iterator;
    }
  }
}

std::vector<const MutationTreeNode*> MutationTreeNode::GetChildren() const {
  std::vector<const MutationTreeNode*> result;
  for (const auto& child : children_) {
    result.push_back(child.get());
  }
  return result;
}

}  // namespace dredd
