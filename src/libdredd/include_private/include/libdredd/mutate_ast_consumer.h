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

#ifndef LIBDREDD_MUTATE_AST_CONSUMER_H
#define LIBDREDD_MUTATE_AST_CONSUMER_H

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/mutate_visitor.h"
#include "libdredd/mutation_tree_node.h"
#include "libdredd/protobufs/dredd_protobufs.h"

namespace dredd {

class MutateAstConsumer : public clang::ASTConsumer {
 public:
  MutateAstConsumer(const clang::CompilerInstance& compiler_instance,
                    bool optimise_mutations, bool dump_ast,
                    bool only_track_mutant_coverage, int& mutation_id,
                    std::optional<protobufs::MutationInfo>& mutation_info)
      : compiler_instance_(&compiler_instance),
        optimise_mutations_(optimise_mutations),
        dump_ast_(dump_ast),
        only_track_mutant_coverage_(only_track_mutant_coverage),
        visitor_(std::make_unique<MutateVisitor>(compiler_instance,
                                                 optimise_mutations)),
        mutation_id_(&mutation_id),
        mutation_info_(&mutation_info) {}

  void HandleTranslationUnit(clang::ASTContext& ast_context) override;

 private:
  [[nodiscard]] std::string GetDreddPreludeCpp(int initial_mutation_id) const;

  [[nodiscard]] std::string GetRegularDreddPreludeCpp(
      int initial_mutation_id) const;

  [[nodiscard]] std::string GetMutantTrackingDreddPreludeCpp(
      int initial_mutation_id) const;

  [[nodiscard]] std::string GetDreddPreludeC(int initial_mutation_id) const;

  [[nodiscard]] std::string GetRegularDreddPreludeC(
      int initial_mutation_id) const;

  [[nodiscard]] std::string GetMutantTrackingDreddPreludeC(
      int initial_mutation_id) const;

  bool RewriteExpressionInMainFileToIntegral(const clang::Expr* expr,
                                             uint64_t x);

  void ApplyMutations(
      const MutationTreeNode& dredd_mutation_tree_node, int initial_mutation_id,
      clang::ASTContext& context,
      protobufs::MutationInfoForFile& protobufs_mutation_info_for_file,
      protobufs::MutationTreeNode& protobufs_mutation_tree_node,
      std::unordered_set<std::string>& dredd_declarations, bool build_tree);

  const clang::CompilerInstance* compiler_instance_;

  // True if and only if Dredd's optimisations are enabled.
  bool optimise_mutations_;

  // True if and only if the AST being consumed should be dumped; useful for
  // debugging.
  bool dump_ast_;

  bool only_track_mutant_coverage_;

  std::unique_ptr<MutateVisitor> visitor_;

  clang::Rewriter rewriter_;

  // Counter used to give each mutation a unique id; shared among AST consumers
  // for different translation units.
  int* mutation_id_;

  std::optional<protobufs::MutationInfo>* mutation_info_;
};

}  // namespace dredd

#endif  // LIBDREDD_MUTATE_AST_CONSUMER_H
