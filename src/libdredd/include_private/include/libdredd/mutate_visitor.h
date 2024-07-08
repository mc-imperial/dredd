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
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/LambdaCapture.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/TemplateBase.h"
#include "clang/AST/Type.h"
#include "clang/AST/TypeLoc.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Frontend/CompilerInstance.h"
#include "libdredd/mutation_tree_node.h"

namespace dredd {

class MutateVisitor : public clang::RecursiveASTVisitor<MutateVisitor> {
 public:
  MutateVisitor(const clang::CompilerInstance& compiler_instance,
                bool optimise_mutations, bool semantics_preserving_mutation);

  bool TraverseDecl(clang::Decl* decl);

  bool TraverseStmt(clang::Stmt* stmt);

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

  // Overridden to avoid mutating array sizes that are derived from template
  // parameters, because after template instantiation these lead to either
  // constant or variable-sized arrays, neither of which can be mutated in C++.
  bool TraverseDependentSizedArrayTypeLoc(
      clang::DependentSizedArrayTypeLoc dependent_sized_array_type_loc);

  // Overridden to avoid mutating template argument expressions, which typically
  // (and perhaps always) need to be compile-time constants.
  bool TraverseTemplateArgumentLoc(
      clang::TemplateArgumentLoc template_argument_loc);

  // Overridden to avoid mutating lambda capture expressions, because the code
  // that can occur in a lambda capture expression is very limited and in
  // particular cannot include other lambdas.
  bool TraverseLambdaCapture(clang::LambdaExpr* lambda_expr,
                             const clang::LambdaCapture* lambda_capture,
                             clang::Expr* init);

  // Overridden to avoid mutating expressions occurring as default values for
  // parameters, because the code that can occur in default values is very
  // limited and cannot include lambdas in general.
  bool TraverseParmVarDecl(clang::ParmVarDecl* parm_var_decl);

  bool VisitExpr(clang::Expr* expr);

  bool TraverseCompoundStmt(clang::CompoundStmt* compound_stmt);

  // Overridden to track all source locations associated with variable
  // declarations, in order to avoid mutating variable declaration reference
  // expressions that collide with the declaration of the variable being
  // referenced (this can happen due to the use of "auto").
  bool VisitVarDecl(clang::VarDecl* var_decl);

  // NOLINTNEXTLINE
  bool shouldTraversePostOrder() { return true; }

  // Should only be called after visitation is complete. Yields the tree of
  // mutations for the translation unit.
  [[nodiscard]] const MutationTreeNode& GetMutations() const {
    return mutation_tree_root_;
  }

  [[nodiscard]] clang::SourceLocation
  GetStartLocationOfFirstFunctionInSourceFile() const {
    return start_location_of_first_function_in_source_file_;
  }

 private:
  // Helper class that uses the RAII pattern to support pushing a new mutation
  // tree node on to the stack of mutation tree nodes used during visitation,
  // and automatically popping the node off the stack when control returns from
  // the visitor method that performed the push.
  class PushMutationTreeRAII {
   public:
    explicit PushMutationTreeRAII(MutateVisitor& mutate_visitor)
        : mutate_visitor_(&mutate_visitor) {
      auto child = std::make_unique<MutationTreeNode>();
      auto* child_ptr = child.get();
      mutate_visitor_->mutation_tree_path_.back()->AddChild(std::move(child));
      mutate_visitor_->mutation_tree_path_.push_back(child_ptr);
    }

    ~PushMutationTreeRAII() { mutate_visitor_->mutation_tree_path_.pop_back(); }

    PushMutationTreeRAII(const PushMutationTreeRAII&) = delete;

    PushMutationTreeRAII& operator=(const PushMutationTreeRAII&) = delete;

    PushMutationTreeRAII(PushMutationTreeRAII&&) = delete;

    PushMutationTreeRAII& operator=(PushMutationTreeRAII&&) = delete;

   private:
    MutateVisitor* mutate_visitor_;
  };

  void HandleUnaryOperator(clang::UnaryOperator* unary_operator);

  void HandleBinaryOperator(clang::BinaryOperator* binary_operator);

  void HandleExpr(clang::Expr* expr);

  static bool IsTypeSupported(clang::QualType qual_type);

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

  void UpdateStartLocationOfFirstFunctionInSourceFile();

  // It is often necessary to ask whether a given statement (which includes
  // expressions) has a parent of a given type. This helper returns nullptr if
  // the given statement has no parent of the template parameter type, and
  // otherwise returns the first parent that does have the template parameter
  // type.
  template <typename RequiredParentT>
  const RequiredParentT* GetFirstParentOfType(const clang::Stmt& stmt) const;

  const clang::CompilerInstance* compiler_instance_;
  bool optimise_mutations_;
  bool semantics_preserving_mutation_;

  // Records the start location of the very first function definition in the
  // source file, before which Dredd's prelude can be placed.
  clang::SourceLocation start_location_of_first_function_in_source_file_;

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

  // Records the mutations that can be applied, in a hierarchical manner.
  MutationTreeNode mutation_tree_root_;

  // Used to keep track of how mutations are hierarchically organised while the
  // AST is being visited.
  std::vector<MutationTreeNode*> mutation_tree_path_;

  // In C++, it is common to introduce a variable in a boolean guard via "auto",
  // and have the guard evaluate to the variable:
  //
  // if (auto x = ...) {
  //   // Use x
  // }
  //
  // The issue here is that while the Clang AST features separate nodes for the
  // declaration of x and its use in the condition of the if statement, these
  // nodes refer to the same source code locations. It is important to avoid
  // mutating the condition to "if (auto __dredd_function(x) = ...)".
  //
  // To avoid this, the set of all source locations for variable declarations is
  // tracked, and mutations are not applied to expression nodes whose start
  // location is one of these locations.
  std::set<clang::SourceLocation> var_decl_source_locations_;
};

}  // namespace dredd

#endif  // LIBDREDD_MUTATE_VISITOR_H
