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

#include "libdredd/mutation_info.h"

#include <map>
#include <ostream>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace dredd {

namespace {

const int kIndentationStep = 2;

void Indent(int depth, std::ostream& json_out) {
  for (int i = 0; i < depth; i++) {
    json_out << " ";
  }
}

}  // namespace

void MutationIdTreeNode::ToJson(int indent, std::ostream& json_out) const {
  Indent(indent, json_out);
  json_out << "{\n";
  Indent(indent + kIndentationStep, json_out);
  json_out << "\"ids\": [ ";
  bool first = true;
  for (int mutation_id : mutation_ids_) {
    if (!first) {
      json_out << ", ";
    }
    first = false;
    json_out << mutation_id;
  }
  json_out << " ],\n";
  Indent(indent + kIndentationStep, json_out);
  json_out << "\"children\": [";
  if (!children_.empty()) {
    json_out << "\n";
    first = true;
    for (const auto& child : children_) {
      if (!first) {
        json_out << ",\n";
      }
      first = false;
      child.ToJson(indent + 2 * kIndentationStep, json_out);
    }
    json_out << "\n";
    Indent(indent + kIndentationStep, json_out);
  }
  json_out << "]\n";
  Indent(indent, json_out);
  json_out << "}";
}

void MutationIdTreeNode::AddMutationId(int mutation_id) {
  mutation_ids_.insert(mutation_id);
}

void MutationIdTreeNode::AddChild(MutationIdTreeNode&& child) {
  children_.push_back(std::move(child));
}

void MutationInfo::AddInfoForFile(std::string file_path,
                                  MutationIdTreeNode&& root) {
  entries_.insert({std::move(file_path), root});
}

void MutationInfo::ToJson(std::ostream& json_out) {
  json_out << "{\n";
  Indent(kIndentationStep, json_out);
  json_out << "\"files\": [";
  bool first = true;
  for (const auto& entry : entries_) {
    if (!first) {
      json_out << ",";
    }
    first = false;
    json_out << "\n";
    Indent(2 * kIndentationStep, json_out);
    json_out << "{\n";
    Indent(3 * kIndentationStep, json_out);
    json_out << R"("filename": ")" << entry.first << "\",\n";
    Indent(3 * kIndentationStep, json_out);
    json_out << "\"mutation_tree\":\n";
    entry.second.ToJson(3 * kIndentationStep, json_out);
    json_out << "\n";
    Indent(2 * kIndentationStep, json_out);
    json_out << "}\n";
  }
  Indent(kIndentationStep, json_out);
  json_out << "]\n";
  json_out << "}\n";
}

}  // namespace dredd
