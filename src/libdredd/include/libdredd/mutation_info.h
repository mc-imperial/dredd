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

#ifndef LIBDREDD_MUTATION_INFO_H
#define LIBDREDD_MUTATION_INFO_H

#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

namespace dredd {

// A MutationIdTreeNode corresponds to an abstract syntax tree node, and stores
// the ids of all mutations that can be applied to that node.
//
// These mutation ids are therefore mutually exclusive -- for example, it
// does not make sense to replace "+" in "a + b" with both "-" and "*". The
// children of a node describe mutations that have been applied to children in
// the abstract syntax tree. Similarly, mutation ids with parent-child
// relationships should be regarded as mutually exclusive. For example, in "a +
// b", it does not make sense to replace the whole expression with 0 and to
// simultaneously replace "a" with 0.
//
// Mutual exclusion between parent and child is conservative; for example, it
// *might* be interesting to replace the "+" in "a + b" with "-" and to also
// replace "b" with 1, but this will be deemed mutually exclusive. This is
// acceptable because (a) there will typically be so many mutations to choose
// from that when enabling multiple mutations for performance, there will be
// plenty of mutations that do not have a parent-child relationship that can be
// paired up, and (b) it seems intuitively more likely that simultaneous
// enabling of mutations that *do* have a parent-child relationship is more
// likely to lead to undesirable conflicts (e.g., mutations cancelling each
// other out) than pairing up mutations in totally disjoint parts of the
// abstract syntax tree.
class MutationIdTreeNode {
 public:
  // Writes a JSON representation of the tree rooted at this node to |json_out|,
  // starting with indentation level |indent|.
  void ToJson(int indent, std::ostream& json_out) const;

  // Adds the given mutation id to the set of ids captured by the tree node.
  void AddMutationId(int mutation_id);

  // Adds the given subtree to this node as a child. An r-value reference is
  // used to ensure that the subtree is moved into this tree.
  void AddChild(MutationIdTreeNode&& child);

 private:
  std::set<int> mutation_ids_;
  std::vector<MutationIdTreeNode> children_;
};

// Represents a mapping from filename to mutation id tree.
class MutationInfo {
 public:
  // Adds a new entry to the mapping. An r-value reference is used for the given
  // tree so that it is moved into this data structure.
  void AddInfoForFile(std::string file_path, MutationIdTreeNode&& root);

  // Writes a JSON representation of the mutation info.
  void ToJson(std::ostream& json_out);

 private:
  std::map<std::string, MutationIdTreeNode> entries_;
};

}  // namespace dredd

#endif  // LIBDREDD_MUTATION_INFO_H
