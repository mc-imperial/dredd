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

#ifndef LIBDREDD_MUTATE_VISITOR_H
#define LIBDREDD_MUTATE_VISITOR_H

#include <memory>
#include <vector>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "libdredd/mutation.h"
#include "libdredd/random_generator.h"

namespace dredd {

class MutateVisitor : public clang::RecursiveASTVisitor<MutateVisitor> {
 public:
  MutateVisitor(const clang::ASTContext& ast_context,
                RandomGenerator& generator);

  bool TraverseDecl(clang::Decl* function_decl);

  bool TraverseFunctionDecl(clang::FunctionDecl* function_decl);

  bool TraverseBinaryOperator(clang::BinaryOperator* binary_operator);

  [[nodiscard]] const std::vector<std::unique_ptr<Mutation>>& GetMutations()
      const {
    return mutations_;
  }

  [[nodiscard]] const clang::Decl* GetFirstDeclInSourceFile() const {
    return first_decl_in_source_file_;
  }

 private:
  template <typename HasSourceRange>
  [[nodiscard]] bool StartsAndEndsInMainSourceFile(
      const HasSourceRange& ast_node) const;

  const clang::ASTContext& ast_context_;

  // Used to randomize the creation of mutations.
  RandomGenerator& generator_;

  // Records the very first declaration in the source file, before which
  // directives such as includes can be placed.
  const clang::Decl* first_decl_in_source_file_;

  // Tracks the function currently being traversed; nullptr if there is no such
  // function.
  const clang::FunctionDecl* enclosing_function_;

  // Records the mutations that can be applied.
  std::vector<std::unique_ptr<Mutation>> mutations_;
};

}  // namespace dredd

#endif  // LIBDREDD_MUTATE_VISITOR_H
