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

#ifndef LIBDREDD_MUTATION_REPLACE_UNARY_OPERATOR_H
#define LIBDREDD_MUTATION_REPLACE_UNARY_OPERATOR_H

#include <sstream>
#include <string>
#include <unordered_set>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/OperationKinds.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/mutation.h"
#include "libdredd/protobufs/dredd_protobufs.h"
#include "libdredd/util.h"

namespace dredd {

class MutationReplaceUnaryOperator : public Mutation {
 public:
  MutationReplaceUnaryOperator(const clang::UnaryOperator& unary_operator,
                               const clang::Preprocessor& preprocessor,
                               const clang::ASTContext& ast_context);

  protobufs::MutationGroup Apply(
      clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
      bool optimise_mutations, bool semantics_preserving_mutation,
      bool only_track_mutant_coverage, int first_mutation_id_in_file,
      int& mutation_id, clang::Rewriter& rewriter,
      std::unordered_set<std::string>& dredd_declarations,
      std::unordered_set<std::string>& dredd_macros) const override;

 private:
  std::string GenerateMutatorFunction(
      clang::ASTContext& ast_context,
      std::unordered_set<std::string>& dredd_macros,
      const std::string& function_name, const std::string& result_type,
      const std::string& input_type, bool optimise_mutations,
      bool semantics_preserving_mutation, bool only_track_mutant_coverage,
      int& mutation_id,
      protobufs::MutationReplaceUnaryOperator& protobuf_message) const;

  [[nodiscard]] static bool IsPrefix(clang::UnaryOperatorKind operator_kind);

  [[nodiscard]] bool IsValidReplacementOperator(
      clang::UnaryOperatorKind operator_kind) const;

  [[nodiscard]] bool IsRedundantReplacementOperator(
      clang::UnaryOperatorKind operator_kind,
      const clang::ASTContext& ast_context) const;

  [[nodiscard]] bool IsOperatorSelfInverse() const;

  std::string GetFunctionName(bool optimise_mutations,
                              clang::ASTContext& ast_context) const;

  static std::string OpKindToString(clang::UnaryOperatorKind kind);

  [[nodiscard]] std::string GetUnaryMacroName(
      const std::string& operator_name, const clang::ASTContext& ast_context,
      bool semantics_preserving_mutation) const;

  [[nodiscard]] std::string GenerateUnaryOperatorReplacementMacro(
      const std::string& name, clang::UnaryOperatorKind operator_kind,
      bool semantics_preserving_mutation,
      const clang::ASTContext& ast_context) const;

  // Replaces unary operators with other valid unary operators.
  void GenerateUnaryOperatorReplacement(
      const std::string& arg_evaluated, const clang::ASTContext& ast_context,
      std::unordered_set<std::string>& dredd_macros, bool optimise_mutations,
      bool semantics_preserving_mutation, bool only_track_mutant_coverage,
      int mutation_id_base, std::stringstream& new_function,
      int& mutation_id_offset,
      protobufs::MutationReplaceUnaryOperator& protobuf_message) const;

  static void AddMutationInstance(
      int mutation_id_base,
      protobufs::MutationReplaceUnaryOperatorAction action,
      int& mutation_id_offset,
      protobufs::MutationReplaceUnaryOperator& protobuf_message);

  [[nodiscard]] static protobufs::MutationReplaceUnaryOperatorAction
  OperatorKindToAction(clang::UnaryOperatorKind operator_kind);

  [[nodiscard]] static protobufs::UnaryOperator
  ClangOperatorKindToProtobufOperatorKind(
      clang::UnaryOperatorKind operator_kind);

  const clang::UnaryOperator* unary_operator_;
  InfoForSourceRange info_for_overall_expr_;
  InfoForSourceRange info_for_sub_expr_;
};

}  // namespace dredd

#endif  // LIBDREDD_MUTATION_REPLACE_UNARY_OPERATOR_H
