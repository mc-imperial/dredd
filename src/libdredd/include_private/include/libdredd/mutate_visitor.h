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
#include "clang/AST/TemplateBase.h"
#include "clang/AST/Type.h"
#include "clang/AST/TypeLoc.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Frontend/CompilerInstance.h"
#include "libdredd/mutation.h"

namespace dredd {

class MutateVisitor : public clang::RecursiveASTVisitor<MutateVisitor> {
 public:
  explicit MutateVisitor(const clang::CompilerInstance& compiler_instance);

  bool TraverseDecl(clang::Decl* decl);

  // Overridden in order to avoid visiting the expressions associated with case
  // statements.
  bool TraverseCaseStmt(clang::CaseStmt* case_stmt);

  // Overridden to avoid mutating constant array size expressions.
  bool TraverseConstantArrayTypeLoc(
      clang::ConstantArrayTypeLoc constant_array_type_loc);

  // Overridden to avoid mutating variable array size expressions in C++
  // (because lambdas cannot appear in such expressions).
  bool TraverseVariableArrayTypeLoc(
      clang::VariableArrayTypeLoc variable_array_type_loc);

  // Overridden to avoid mutating variable array size expressions in C++
  // (because lambdas cannot appear in such expressions).
  bool TraverseDependentSizedArrayTypeLoc(
      clang::DependentSizedArrayTypeLoc dependent_sized_array_type_loc);

  // Overridden to avoid mutating template argument expressions, which typically
  // (and perhaps always) need to be compile-time constants.
  bool TraverseTemplateArgumentLoc(
      clang::TemplateArgumentLoc template_argument_loc);

  bool VisitUnaryOperator(clang::UnaryOperator* unary_operator);

  bool VisitBinaryOperator(clang::BinaryOperator* binary_operator);

  bool VisitExpr(clang::Expr* expr);

  bool VisitCompoundStmt(clang::CompoundStmt* compound_stmt);

  static bool IsTypeSupported(clang::QualType qual_type);

  // NOLINTNEXTLINE
  bool shouldTraversePostOrder() { return true; }

  [[nodiscard]] const std::vector<std::unique_ptr<Mutation>>& GetMutations()
      const {
    return mutations_;
  }

  [[nodiscard]] clang::SourceLocation GetStartLocationOfFirstDeclInSourceFile()
      const {
    return start_location_of_first_decl_in_source_file_;
  }

 private:
  const clang::CompilerInstance& compiler_instance_;

  // Records the start locat of the very first declaration in the source file,
  // before which Dredd's prelude can be placed.
  clang::SourceLocation start_location_of_first_decl_in_source_file_;

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

  // Determines whether the AST node being visited is directly inside a
  // function, allowing for the visitation point to be inside a variable
  // declaration as long as that declaration is itself directly inside a
  // function. This should return true in cases such as:
  //
  // void foo() {
  //   (*)
  // }
  //
  // and cases such as:
  //
  // void foo() {
  //   int x = (*);
  // }
  //
  // but should return false in cases such as:
  //
  // void foo() {
  //   class A {
  //     static int x = (*);
  //   };
  // }
  bool IsInFunction();
};

}  // namespace dredd

#endif  // LIBDREDD_MUTATE_VISITOR_H
