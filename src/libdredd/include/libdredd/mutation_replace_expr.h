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

#ifndef LIBDREDD_MUTATION_REPLACE_EXPR_H_
#define LIBDREDD_MUTATION_REPLACE_EXPR_H_

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

class MutationReplaceExpr : public Mutation {
 public:
  MutationReplaceExpr(const clang::Expr& expr,
                      const clang::Preprocessor& preprocessor,
                      const clang::ASTContext& ast_context);

  protobufs::MutationGroup Apply(
      clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
      bool optimise_mutations, bool semantics_preserving_mutation,
      bool only_track_mutant_coverage, int first_mutation_id_in_file,
      int& mutation_id, clang::Rewriter& rewriter,
      std::unordered_set<std::string>& dredd_declarations,
      std::unordered_set<std::string>& dredd_macros) const override;

  [[nodiscard]] static std::string GetExprMacroName(
      const std::string& operator_name);

  static void ApplyCppTypeModifiers(const clang::Expr& expr, std::string& type);

  static void ApplyCTypeModifiers(const clang::Expr& expr, std::string& type);

  // Check if an expression is equivalent to a constant.
  static bool ExprIsEquivalentToInt(const clang::Expr& expr, int constant,
                                    const clang::ASTContext& ast_context);
  static bool ExprIsEquivalentToFloat(const clang::Expr& expr, double constant,
                                      const clang::ASTContext& ast_context);
  static bool ExprIsEquivalentToBool(const clang::Expr& expr, bool constant,
                                     const clang::ASTContext& ast_context);

  // L-value expressions can be mutated via insertion of the ++ and -- prefix
  // operators. This is only done when an l-value is about to be implicitly
  // converted to an r-value. This works well in C, where these operators do not
  // return an l-value, and should also provide a reasonable degree of mutation
  // of l-values for C++.
  //
  // This helper function determines when an l-value expression is suitable for
  // such a mutation.
  static bool CanMutateLValue(clang::ASTContext& ast_context,
                              const clang::Expr& expr);

 private:
  [[nodiscard]] bool IsRedundantOperatorInsertion(
      clang::ASTContext& ast_context,
      clang::UnaryOperatorKind operator_kind) const;

  [[nodiscard]] bool IsBooleanReplacementRedundantForBinaryOperator(
      bool replacement_value, const clang::ASTContext& ast_context) const;

  [[nodiscard]] bool IsRedundantOperatorInsertionBeforeBinaryExpr(
      const clang::ASTContext& ast_context) const;

  [[nodiscard]] bool IsRedundantOperatorInsertionBeforeLogicalOperatorArgument(
      clang::ASTContext& ast_context) const;

  [[nodiscard]] bool IsRedundantUnaryLogicalNotInsertion(
      const clang::ASTContext& ast_context) const;

  [[nodiscard]] bool IsRedundantUnaryNotInsertion(
      const clang::ASTContext& ast_context) const;

  [[nodiscard]] bool IsRedundantUnaryMinusInsertion(
      const clang::ASTContext& ast_context) const;

  void AddOptimisationSpecifier(clang::ASTContext& ast_context,
                                std::string& function_name) const;

  // Replace expressions with constants.
  void GenerateConstantReplacement(
      clang::ASTContext& ast_context,
      std::unordered_set<std::string>& dredd_macros, bool optimise_mutations,
      bool semantics_preserving_mutation, bool only_track_mutant_coverage,
      int mutation_id_base, std::stringstream& new_function,
      int& mutation_id_offset,
      protobufs::MutationReplaceExpr& protobuf_message) const;

  void GenerateBooleanConstantReplacement(
      clang::ASTContext& ast_context,
      std::unordered_set<std::string>& dredd_macros, bool optimise_mutations,
      bool semantics_preserving_mutation, bool only_track_mutant_coverage,
      int mutation_id_base, std::stringstream& new_function,
      int& mutation_id_offset,
      protobufs::MutationReplaceExpr& protobuf_message) const;

  void GenerateIntegerConstantReplacement(
      const clang::ASTContext& ast_context,
      std::unordered_set<std::string>& dredd_macros, bool optimise_mutations,
      bool semantics_preserving_mutation, bool only_track_mutant_coverage,
      int mutation_id_base, std::stringstream& new_function,
      int& mutation_id_offset,
      protobufs::MutationReplaceExpr& protobuf_message) const;

  void GenerateFloatConstantReplacement(
      const clang::ASTContext& ast_context,
      std::unordered_set<std::string>& dredd_macros, bool optimise_mutations,
      bool semantics_preserving_mutation, bool only_track_mutant_coverage,
      int mutation_id_base, std::stringstream& new_function,
      int& mutation_id_offset,
      protobufs::MutationReplaceExpr& protobuf_message) const;

  // Insert valid unary operators such as !, ~, ++ and --.
  void GenerateUnaryOperatorInsertion(
      const std::string& arg_evaluated, clang::ASTContext& ast_context,
      std::unordered_set<std::string>& dredd_macros, bool optimise_mutations,
      bool semantics_preserving_mutation, bool only_track_mutant_coverage,
      int mutation_id_base, std::stringstream& new_function,
      int& mutation_id_offset,
      protobufs::MutationReplaceExpr& protobuf_message) const;

  void GenerateUnaryOperatorInsertionBeforeNonLValue(
      const std::string& arg_evaluated, clang::ASTContext& ast_context,
      std::unordered_set<std::string>& dredd_macros, bool optimise_mutations,
      bool semantics_preserving_mutation, bool only_track_mutant_coverage,
      int mutation_id_base, std::stringstream& new_function,
      int& mutation_id_offset,
      protobufs::MutationReplaceExpr& protobuf_message) const;

  void GenerateUnaryOperatorInsertionBeforeLValue(
      const std::string& arg_evaluated, clang::ASTContext& ast_context,
      std::unordered_set<std::string>& dredd_macros,
      bool semantics_preserving_mutation, bool only_track_mutant_coverage,
      int mutation_id_base, std::stringstream& new_function,
      int& mutation_id_offset,
      protobufs::MutationReplaceExpr& protobuf_message) const;

  std::string GenerateMutatorFunction(
      clang::ASTContext& ast_context,
      std::unordered_set<std::string>& dredd_macros,
      const std::string& function_name, const std::string& result_type,
      const std::string& input_type, bool optimise_mutations,
      bool semantics_preserving_mutation, bool only_track_mutant_coverage,
      int& mutation_id, protobufs::MutationReplaceExpr& protobuf_message) const;

  [[nodiscard]] std::string GetFunctionName(
      bool optimise_mutations, clang::ASTContext& ast_context) const;

  void ReplaceExprWithFunctionCall(const std::string& new_function_name,
                                   const std::string& input_type,
                                   bool semantics_preserving_mutation,
                                   int local_mutation_id,
                                   clang::ASTContext& ast_context,
                                   const clang::Preprocessor& preprocessor,
                                   clang::Rewriter& rewriter) const;

  static void AddMutationInstance(
      int mutation_id_base, protobufs::MutationReplaceExprAction action,
      int& mutation_id_offset,
      protobufs::MutationReplaceExpr& protobuf_message);

  const clang::Expr* expr_;
  InfoForSourceRange info_for_source_range_;
};

}  // namespace dredd

#endif  // LIBDREDD_MUTATION_REPLACE_EXPR_H_
