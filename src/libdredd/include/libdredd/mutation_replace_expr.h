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
#include "clang/AST/Type.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/mutation.h"

namespace dredd {

class MutationReplaceExpr : public Mutation {
 public:
  explicit MutationReplaceExpr(const clang::Expr& expr);

  void Apply(
      clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
      int first_mutation_id_in_file, int& mutation_id,
      clang::Rewriter& rewriter,
      std::unordered_set<std::string>& dredd_declarations) const override;

  // Replace expressions with constants.
  static void GenerateConstantReplacement(const clang::Expr& expr_,
                                          const clang::BuiltinType& exprType,
                                          clang::ASTContext& ast_context,
                                          std::stringstream& new_function,
                                          int& mutant_offset);

  // Insert valid unary operators such as !, ~, ++ and --.
  static void GenerateUnaryOperatorInsertion(const std::string& arg_evaluated,
                                             const clang::Expr& expr_,
                                             const clang::BuiltinType& exprType,
                                             std::stringstream& new_function,
                                             int& mutant_offset);

 private:
  std::string GenerateMutatorFunction(clang::ASTContext& ast_context,
                                      const std::string& function_name,
                                      const std::string& result_type,
                                      const std::string& input_type,
                                      int& mutation_id) const;

  static void ApplyCppTypeModifiers(const clang::Expr *expr, std::string &type);
  static void ApplyCTypeModifiers(const clang::Expr *expr, std::string &type);

  [[nodiscard]] std::string GetFunctionName(clang::ASTContext& ast_context) const;

  const clang::Expr& expr_;
};

}  // namespace dredd

#endif  // LIBDREDD_MUTATION_REPLACE_EXPR_H_