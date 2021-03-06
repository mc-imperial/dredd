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

#include <string>
#include <unordered_set>
#include <vector>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/OperationKinds.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/mutation.h"

namespace dredd {

class MutationReplaceBinaryOperator : public Mutation {
 public:
  explicit MutationReplaceBinaryOperator(
      const clang::BinaryOperator& binary_operator);

  void Apply(
      clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
      int first_mutation_id_in_file, int& mutation_id,
      clang::Rewriter& rewriter,
      std::unordered_set<std::string>& dredd_declarations) const override;

 private:
  std::string GenerateMutatorFunction(
      const std::string& function_name, const std::string& result_type,
      const std::string& lhs_type, const std::string& rhs_type,
      const std::vector<clang::BinaryOperatorKind>& operators,
      int& mutation_id) const;

  [[nodiscard]] bool IsValidReplacementOperator(
      clang::BinaryOperatorKind op) const;

  const clang::BinaryOperator& binary_operator_;
};

}  // namespace dredd

#endif  // LIBDREDD_MUTATION_REPLACE_BINARY_OPERATOR_H
