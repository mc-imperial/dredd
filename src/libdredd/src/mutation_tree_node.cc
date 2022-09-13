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

MutationTreeNode& MutationTreeNode::AddChild(MutationTreeNode&& node) {
  children_.push_back(std::move(node));
  return children_.back();
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
    if (!child.IsEmpty()) {
      return false;
    }
  }
  return true;
}

void MutationTreeNode::Compress() {
  while (mutations_.empty() && children_.size() == 1) {
    mutations_ = std::move(children_[0].mutations_);
    children_ = std::move(children_[0].children_);
  }
  for (auto& child : children_) {
    child.Compress();
  }
}

void MutationTreeNode::PruneEmptySubtrees() {
  auto it = children_.begin();
  while (it != children_.end()) {
    if (it->IsEmpty()) {
      it = children_.erase(it);
    } else {
      it->PruneEmptySubtrees();
      ++it;
    }
  }
}

}  // namespace dredd
