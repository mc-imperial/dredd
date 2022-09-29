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
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/mutation.h"
#include "libdredd/protobufs/protobufs.h"

namespace dredd {

class MutationReplaceExpr : public Mutation {
 public:
  explicit MutationReplaceExpr(const clang::Expr& expr);

  protobufs::MutationGroup Apply(
      clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
      bool optimise_mutations, int first_mutation_id_in_file, int& mutation_id,
      clang::Rewriter& rewriter,
      std::unordered_set<std::string>& dredd_declarations) const override;

  static void ApplyCppTypeModifiers(const clang::Expr* expr, std::string& type);

  static void ApplyCTypeModifiers(const clang::Expr* expr, std::string& type);

  // Check if an expression is equivalent to a constant.
  static bool ExprIsEquivalentToInt(const clang::Expr& expr, int constant,
                                    clang::ASTContext& ast_context);
  static bool ExprIsEquivalentToFloat(const clang::Expr& expr, double constant,
                                      clang::ASTContext& ast_context);
  static bool ExprIsEquivalentToBool(const clang::Expr& expr, bool constant,
                                     clang::ASTContext& ast_context);

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
  static bool IsRedundantOperatorInsertion(const clang::Expr& expr,
                                           clang::ASTContext& ast_context);

  void AddOptimisationSpecifier(clang::ASTContext& ast_context,
                                std::string& function_name) const;

  // Replace expressions with constants.
  void GenerateConstantReplacement(clang::ASTContext& ast_context,
                                   bool optimise_mutations,
                                   std::stringstream& new_function,
                                   int& mutant_offset) const;

  void GenerateBooleanConstantReplacement(clang::ASTContext& ast_context,
                                          bool optimise_mutations,
                                          std::stringstream& new_function,
                                          int& mutant_offset) const;

  void GenerateIntegerConstantReplacement(clang::ASTContext& ast_context,
                                          bool optimise_mutations,
                                          std::stringstream& new_function,
                                          int& mutant_offset) const;

  void GenerateFloatConstantReplacement(clang::ASTContext& ast_context,
                                        bool optimise_mutations,
                                        std::stringstream& new_function,
                                        int& mutant_offset) const;

  // Insert valid unary operators such as !, ~, ++ and --.
  void GenerateUnaryOperatorInsertion(const std::string& arg_evaluated,
                                      clang::ASTContext& ast_context,
                                      bool optimise_mutations,
                                      std::stringstream& new_function,
                                      int& mutant_offset) const;

  std::string GenerateMutatorFunction(clang::ASTContext& ast_context,
                                      const std::string& function_name,
                                      const std::string& result_type,
                                      const std::string& input_type,
                                      bool optimise_mutations,
                                      int& mutation_id) const;

  [[nodiscard]] std::string GetFunctionName(
      bool optimise_mutations, clang::ASTContext& ast_context) const;

  const clang::Expr& expr_;
};

}  // namespace dredd

#endif  // LIBDREDD_MUTATION_REPLACE_EXPR_H_
