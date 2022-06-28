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
#include <unordered_set>
#include <vector>

#include "clang/AST/DeclBase.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/Frontend/CompilerInstance.h"
#include "libdredd/mutation.h"

namespace dredd {

class MutateVisitor : public clang::RecursiveASTVisitor<MutateVisitor> {
 public:
  explicit MutateVisitor(const clang::CompilerInstance& compiler_instance);

  bool TraverseDecl(clang::Decl* decl);

  bool VisitBinaryOperator(clang::BinaryOperator* binary_operator);

  bool VisitCompoundStmt(clang::CompoundStmt* compound_stmt);

  // NOLINTNEXTLINE
  bool shouldTraversePostOrder() { return true; }

  [[nodiscard]] const std::vector<std::unique_ptr<Mutation>>& GetMutations()
      const {
    return mutations_;
  }

  [[nodiscard]] const clang::Decl* GetFirstDeclInSourceFile() const {
    return first_decl_in_source_file_;
  }

 private:
  const clang::CompilerInstance& compiler_instance_;

  // Records the very first declaration in the source file, before which
  // directives such as includes can be placed.
  const clang::Decl* first_decl_in_source_file_;

  // Tracks the nest of declarations currently being traversed. Any new Dredd
  // functions will be put before the start of the current nest, which avoids
  // e.g. putting a Dredd function inside a class or function.
  std::vector<const clang::Decl*> enclosing_decls_;

  // These fields track whether a statement contains some sub-statement that
  // might cause control to branch outside of the statement. This needs to be
  // tracked to determine when it is legitimate to move a statement into a
  // lambda to simulate statement removal.
  std::unordered_set<clang::Stmt*> contains_return_goto_or_label_;
  std::unordered_set<clang::Stmt*> contains_break_for_enclosing_loop_or_switch_;
  std::unordered_set<clang::Stmt*> contains_continue_for_enclosing_loop_;
  std::unordered_set<clang::Stmt*> contains_case_for_enclosing_switch_;

  // Records the mutations that can be applied.
  std::vector<std::unique_ptr<Mutation>> mutations_;
};

}  // namespace dredd

#endif  // LIBDREDD_MUTATE_VISITOR_H
