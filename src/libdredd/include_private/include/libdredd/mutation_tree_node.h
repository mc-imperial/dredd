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

#ifndef LIBDREDD_MUTATION_TREE_NODE_H
#define LIBDREDD_MUTATION_TREE_NODE_H

#include <memory>
#include <vector>

#include "libdredd/mutation.h"

namespace dredd {

// Used to track the hierarchical structure between mutations as they are
// created during the traversal of an abstract syntax tree. Subsequently, the
// root MutationTreeNode for a translation unit is turned into a
// MutationIdTreeNode, which just captures the ids associated with the sets of
// mutations instantiated by each Mutation object.
class MutationTreeNode {
 public:
  // Adds the given mutation to this tree node.
  void AddMutation(std::unique_ptr<Mutation> mutation);

  // Moves the given subtree into this tree node as a child.
  MutationTreeNode& AddChild(MutationTreeNode&& node);

  // Yields the children of this tree node.
  [[nodiscard]] const std::vector<MutationTreeNode>& GetChildren() const {
    return children_;
  }

  // Yields the mutations associated with this tree node.
  [[nodiscard]] const std::vector<std::unique_ptr<Mutation>>& GetMutations()
      const {
    return mutations_;
  }

  // Once the tree has been built, this method should be invoked to make it
  // simpler, without losing any relationship between mutations. This includes,
  // for example, pruning empty subtrees.
  void TidyUp();

  // Returns true if and only if every node in the subtree rooted at this node
  // contains no mutations.
  [[nodiscard]] bool IsEmpty() const;

 private:
  // Used by "TidyUp" to squash chains of nodes that hold no mutations and only
  // have one child.
  void Compress();

  // Used by "TidyUp" to eliminate empty subtrees.
  void PruneEmptySubtrees();

  std::vector<std::unique_ptr<Mutation>> mutations_;
  std::vector<MutationTreeNode> children_;
};

}  // namespace dredd

#endif  // LIBDREDD_MUTATION_TREE_NODE_H
