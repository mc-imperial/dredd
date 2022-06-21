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

  bool VisitBinaryOperator(clang::BinaryOperator* binary_operator);

  bool VisitCompoundStmt(clang::CompoundStmt* compound_stmt);

  bool VisitReturnStmt(clang::ReturnStmt* return_stmt);

  bool VisitBreakStmt(clang::BreakStmt* break_stmt);

  bool VisitContinueStmt(clang::ContinueStmt* continue_stmt);

  bool VisitGotoStmt(clang::GotoStmt* goto_stmt);

  bool VisitLabelStmt(clang::LabelStmt* label_stmt);

  bool VisitSwitchCase(clang::SwitchCase* switch_case);

  bool VisitIfStmt(clang::IfStmt* if_stmt);

  bool VisitForStmt(clang::ForStmt* for_stmt);

  bool VisitWhileStmt(clang::WhileStmt* while_stmt);

  bool VisitDoStmt(clang::DoStmt* do_stmt);

  bool VisitSwitchStmt(clang::SwitchStmt* switch_stmt);

  bool shouldTraversePostOrder() { return true; }

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

  std::unordered_set<clang::Stmt*> contains_return_goto_or_label_;

  std::unordered_set<clang::Stmt*> contains_break_for_enclosing_loop_or_switch_;

  std::unordered_set<clang::Stmt*> contains_continue_for_enclosing_loop_;

  std::unordered_set<clang::Stmt*> contains_case_for_enclosing_switch_;

  // Records the mutations that can be applied.
  std::vector<std::unique_ptr<Mutation>> mutations_;
};

}  // namespace dredd

#endif  // LIBDREDD_MUTATE_VISITOR_H
