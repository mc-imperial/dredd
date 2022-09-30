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

#ifndef LIBDREDD_MUTATION_REPLACE_BINARY_OPERATOR_H
#define LIBDREDD_MUTATION_REPLACE_BINARY_OPERATOR_H

#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/OperationKinds.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/mutation.h"
#include "libdredd/protobufs/dredd_protobufs.h"
#include "libdredd/util.h"

namespace dredd {

class MutationReplaceBinaryOperator : public Mutation {
 public:
  MutationReplaceBinaryOperator(const clang::BinaryOperator& binary_operator,
                                const clang::Preprocessor& preprocessor,
                                const clang::ASTContext& ast_context);

  protobufs::MutationGroup Apply(
      clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
      bool optimise_mutations, int first_mutation_id_in_file, int& mutation_id,
      clang::Rewriter& rewriter,
      std::unordered_set<std::string>& dredd_declarations) const override;

 private:
  std::string GenerateMutatorFunction(
      clang::ASTContext& ast_context, const std::string& function_name,
      const std::string& result_type, const std::string& lhs_type,
      const std::string& rhs_type, bool optimise_mutations,
      const std::vector<clang::BinaryOperatorKind>& operators, int& mutation_id,
      protobufs::MutationReplaceBinaryOperator& protobuf_message) const;

  void ReplaceOperator(const std::string& lhs_type, const std::string& rhs_type,
                       const std::string& new_function_name,
                       clang::ASTContext& ast_context,
                       const clang::Preprocessor& preprocessor,
                       int first_mutation_id_in_file, int& mutation_id,
                       clang::Rewriter& rewriter) const;

  std::string GetFunctionName(bool optimise_mutations,
                              clang::ASTContext& ast_context) const;

  [[nodiscard]] bool IsRedundantReplacementOperator(
      clang::BinaryOperatorKind operator_kind,
      clang::ASTContext& ast_context) const;

  [[nodiscard]] bool IsValidReplacementOperator(
      clang::BinaryOperatorKind operator_kind) const;

  // This returns a string corresponding to the non-mutated expression.
  std::string GetExpr(clang::ASTContext& ast_context) const;

  // Replaces binary expressions with either the left or right operand.
  void GenerateArgumentReplacement(
      const std::string& arg1_evaluated, const std::string& arg2_evaluated,
      clang::ASTContext& ast_context, bool optimise_mutations,
      int mutation_id_base, std::stringstream& new_function,
      int& mutation_id_offset,
      protobufs::MutationReplaceBinaryOperator& protobuf_message) const;

  // Replaces binary operators with other valid binary operators.
  void GenerateBinaryOperatorReplacement(
      const std::vector<clang::BinaryOperatorKind>& operators,
      const std::string& arg1_evaluated, const std::string& arg2_evaluated,
      clang::ASTContext& ast_context, bool optimise_mutations,
      int mutation_id_base, std::stringstream& new_function,
      int& mutation_id_offset,
      protobufs::MutationReplaceBinaryOperator& protobuf_message) const;

  // The && and || operators in C require special treatment: due to
  // short-circuit evaluation their arguments must not be prematurely evaluated.
  // In C++ this is worked around via lambdas, but lambdas are not available in
  // C.
  //
  // The C workaround is, conceptually, to achieve replacements as follows:
  //  - "a && b" -> "a || b" via "!(!a && !b)"
  //  - "a && b" -> "a" via "a && 1"
  //  - "a && b" -> "b" via "1 && a"
  // with analogous replacements for "||".
  //
  // Using these rewritings avoid the need to change when the operator arguments
  // are evaluated when no mutations are enabled.
  void HandleCLogicalOperator(
      const clang::Preprocessor& preprocessor,
      const std::string& new_function_prefix, const std::string& result_type,
      const std::string& lhs_type, const std::string& rhs_type,
      int first_mutation_id_in_file, int& mutation_id,
      clang::Rewriter& rewriter,
      std::unordered_set<std::string>& dredd_declarations) const;

  const clang::BinaryOperator& binary_operator_;
  const InfoForSourceRange info_for_overall_expr_;
  const InfoForSourceRange info_for_lhs_;
  const InfoForSourceRange info_for_rhs_;
};

}  // namespace dredd

#endif  // LIBDREDD_MUTATION_REPLACE_BINARY_OPERATOR_H
