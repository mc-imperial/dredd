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

#include "libdredd/mutate_visitor.h"

#include <cassert>
#include <cstddef>
#include <memory>

#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/LambdaCapture.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/TemplateBase.h"
#include "clang/AST/Type.h"
#include "clang/AST/TypeLoc.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "libdredd/mutation.h"
#include "libdredd/mutation_remove_statement.h"
#include "libdredd/mutation_replace_binary_operator.h"
#include "libdredd/mutation_replace_expr.h"
#include "libdredd/mutation_replace_unary_operator.h"
#include "libdredd/util.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/Casting.h"

namespace dredd {

MutateVisitor::MutateVisitor(clang::CompilerInstance& compiler_instance,
                             bool optimise_mutations)
    : compiler_instance_(compiler_instance),
      optimise_mutations_(optimise_mutations),
      mutation_tree_root_() {
  mutation_tree_path_.push_back(&mutation_tree_root_);
}

bool MutateVisitor::IsTypeSupported(const clang::QualType qual_type) {
  const auto* builtin_type = qual_type->getAs<clang::BuiltinType>();
  return builtin_type != nullptr &&
         (builtin_type->isInteger() || builtin_type->isFloatingPoint());
}

bool MutateVisitor::IsInFunction() {
  // Walk up the next of enclosing declarations
  for (int index = static_cast<int>(enclosing_decls_.size()) - 1; index >= 0;
       index--) {
    const auto* decl = enclosing_decls_[static_cast<size_t>(index)];
    if (llvm::dyn_cast<clang::FunctionDecl>(decl) != nullptr) {
      // A function declaration has been found, either directly or only by going
      // via variable declarations. Thus the point of visitation is in a
      // function without any other intervening constructs.
      return true;
    }
    // It is OK if the visitation point is inside a variable declaration, as
    // long as that declaration turns out to be inside a function.
    if (llvm::dyn_cast<clang::VarDecl>(decl) == nullptr) {
      // The visitation point is inside some other declaration (e.g. a class).
      return false;
    }
  }
  // Global scope was reached without hitting a function, so the declaration is
  // not in a function.
  return false;
}

bool MutateVisitor::TraverseDecl(clang::Decl* decl) {
  if (llvm::dyn_cast<clang::TranslationUnitDecl>(decl) != nullptr) {
    // This is the top-level translation unit declaration, so descend into it.
    bool result = RecursiveASTVisitor::TraverseDecl(decl);
    // At this point the translation unit has been fully visited, so the
    // mutation tree that has been built can be made simpler, in preparation for
    // later turning it into a JSON summary.
    mutation_tree_root_.TidyUp();
    return result;
  }
  auto source_range_in_main_file =
      GetSourceRangeInMainFile(compiler_instance_.getPreprocessor(), *decl);
  if (source_range_in_main_file.isInvalid()) {
    // This declaration is not wholly contained in the main file, so do not
    // consider it for mutation.
    return true;
  }
  clang::BeforeThanCompare<clang::SourceLocation> comparator(
      compiler_instance_.getSourceManager());
  if (start_location_of_first_decl_in_source_file_.isInvalid() ||
      comparator(source_range_in_main_file.getBegin(),
                 start_location_of_first_decl_in_source_file_)) {
    // This is the first declaration wholly contained in the main file that has
    // been encountered so far: record it so that the dredd prelude can be
    // inserted before it.
    //
    // The order in which declarations appear in the source file may not exactly
    // match the order they are visited in the AST (for example, a typedef
    // declaration is visited after the associated type declaration, even though
    // the 'typedef' keyword occurs first in the AST), thus this location is
    // updated each time a declaration that appears earlier is encountered.
    start_location_of_first_decl_in_source_file_ =
        source_range_in_main_file.getBegin();
  }
  if (llvm::dyn_cast<clang::StaticAssertDecl>(decl) != nullptr) {
    // It does not make sense to mutate static assertions, as (a) this will
    // very likely lead to compile-time failures due to the assertion not
    // holding, (b) if compilation succeeds then the assertion is not actually
    // present at runtime so there is no notion of killing the mutant, and (c)
    // dynamic instrumentation of the mutation operator will break the rules
    // associated with static assertions anyway.
    return true;
  }
  if (const auto* var_decl = llvm::dyn_cast<clang::VarDecl>(decl)) {
    if (var_decl->isConstexpr()) {
      // Because Dredd's mutations occur dynamically, they cannot be applied to
      // C++ constexprs, which require compile-time evaluation.
      return true;
    }
    if (!compiler_instance_.getLangOpts().CPlusPlus &&
        var_decl->isStaticLocal()) {
      // In C, static local variables can only be initialized using constant
      // expressions, which require compile-time evaluation.
      return true;
    }
  }

  enclosing_decls_.push_back(decl);
  // Consider the declaration for mutation.
  RecursiveASTVisitor::TraverseDecl(decl);
  enclosing_decls_.pop_back();

  return true;
}

bool MutateVisitor::TraverseStmt(clang::Stmt* stmt) {
  // Add a node to the mutation tree to capture any mutations beneath this
  // statement.
  PushMutationTreeRAII push_mutation_tree(*this);
  return RecursiveASTVisitor::TraverseStmt(stmt);
}

bool MutateVisitor::TraverseCaseStmt(clang::CaseStmt* case_stmt) {
  // Do not traverse the expression associated with this switch case: switch
  // case expressions need to be constant, which rules out the kinds of
  // mutations that Dredd performs.
  return TraverseStmt(case_stmt->getSubStmt());
}

bool MutateVisitor::TraverseConstantArrayTypeLoc(
    clang::ConstantArrayTypeLoc constant_array_type_loc) {
  // Prevent compilers complaining that this method could be made static, and
  // that it ignores its parameter.
  (void)this;
  (void)constant_array_type_loc;
  // Changing a constant-sized array to a non-constant-sized array is
  // problematic in C if the array has an initializer, and in C++ lambdas cannot
  // be used in array size expressions. For simplicity, don't try to mutate
  // constant array sizes.
  return true;
}

bool MutateVisitor::TraverseVariableArrayTypeLoc(
    clang::VariableArrayTypeLoc variable_array_type_loc) {
  if (compiler_instance_.getLangOpts().CPlusPlus) {
    // In C++, lambdas cannot appear in array sizes, so avoid mutating here.
    return true;
  }
  return RecursiveASTVisitor::TraverseVariableArrayTypeLoc(
      variable_array_type_loc);
}

bool MutateVisitor::TraverseDependentSizedArrayTypeLoc(
    clang::DependentSizedArrayTypeLoc dependent_sized_array_type_loc) {
  // Prevent compilers complaining that this method could be made static, and
  // that it ignores its parameter.
  (void)this;
  (void)dependent_sized_array_type_loc;
  // Changing an array whose size is derived from template parameters is
  // problematic because after template instantiation these lead to either
  // constant or variable-sized arrays, neither of which can be mutated in C++.
  return true;
}

bool MutateVisitor::TraverseTemplateArgumentLoc(
    clang::TemplateArgumentLoc template_argument_loc) {
  // Prevent compilers complaining that this method could be made static, and
  // that it ignores its parameter.
  (void)this;
  (void)template_argument_loc;
  // C++ template arguments typically need to be compile-time constants, and so
  // should not be mutated.
  return true;
}

bool MutateVisitor::TraverseLambdaCapture(
    clang::LambdaExpr* lambda_expr, const clang::LambdaCapture* lambda_capture,
    clang::Expr* init) {
  // Prevent compilers complaining that this method could be made static, and
  // that it ignores its parameters.
  (void)this;
  (void)lambda_expr;
  (void)lambda_capture;
  (void)init;
  return true;
}

bool MutateVisitor::TraverseParmVarDecl(clang::ParmVarDecl* parm_var_decl) {
  // Prevent compilers complaining that this method could be made static, and
  // that it ignores its parameter.
  (void)this;
  (void)parm_var_decl;
  return true;
}

bool MutateVisitor::HandleUnaryOperator(clang::UnaryOperator* unary_operator) {
  // Check that the argument to the unary expression has a source ranges that is
  // part of the main file. In particular, this avoids mutating expressions that
  // directly involve the use of macros (though it is OK if sub-expressions of
  // arguments use macros).
  if (GetSourceRangeInMainFile(compiler_instance_.getPreprocessor(),
                               *unary_operator->getSubExpr())
          .isInvalid()) {
    return true;
  }

  // Don't mutate unary plus as this is unlikely to lead to a mutation that
  // differs from inserting a unary operator
  if (unary_operator->getOpcode() == clang::UnaryOperatorKind::UO_Plus) {
    return true;
  }

  // Check that the argument type is supported
  if (!IsTypeSupported(unary_operator->getSubExpr()->getType())) {
    return true;
  }

  // As it is not possible to pass bit-fields by reference, mutation of
  // bit-field l-values is not supported.
  if (unary_operator->getSubExpr()->refersToBitField()) {
    return true;
  }

  mutation_tree_path_.back()->AddMutation(
      std::make_unique<MutationReplaceUnaryOperator>(*unary_operator));
  return true;
}

bool MutateVisitor::HandleBinaryOperator(
    clang::BinaryOperator* binary_operator) {
  // Check that arguments of the binary expression have source ranges that are
  // part of the main file. In particular, this avoids mutating expressions that
  // directly involve the use of macros (though it is OK if sub-expressions of
  // arguments use macros).
  if (GetSourceRangeInMainFile(compiler_instance_.getPreprocessor(),
                               *binary_operator->getLHS())
          .isInvalid() ||
      GetSourceRangeInMainFile(compiler_instance_.getPreprocessor(),
                               *binary_operator->getRHS())
          .isInvalid()) {
    return true;
  }

  // We only want to change operators for binary operations on basic types.
  // Check that the argument types are supported.
  if (!IsTypeSupported(binary_operator->getLHS()->getType())) {
    return true;
  }
  if (!IsTypeSupported(binary_operator->getRHS()->getType())) {
    return true;
  }

  // As it is not possible to pass bit-fields by reference, mutation of
  // bit-field l-values is not supported.
  if (binary_operator->getLHS()->refersToBitField()) {
    return true;
  }

  if (binary_operator->isCommaOp()) {
    // The comma operator is so versatile that it does not make a great deal of
    // sense to try to rewrite it.
    return true;
  }

  // There is no useful way to mutate this expression since it is equivalent to
  // replacement with a constant in all cases.
  if (optimise_mutations_ &&
      MutationReplaceExpr::ExprIsEquivalentTo(
          *binary_operator->getLHS(), 0, compiler_instance_.getASTContext()) &&
      MutationReplaceExpr::ExprIsEquivalentTo(
          *binary_operator->getRHS(), 1, compiler_instance_.getASTContext())) {
    return true;
  }

  mutation_tree_path_.back()->AddMutation(
      std::make_unique<MutationReplaceBinaryOperator>(*binary_operator));
  return true;
}

bool MutateVisitor::VisitExpr(clang::Expr* expr) {
  // There is no value in mutating a parentheses expression.
  if (llvm::dyn_cast<clang::ParenExpr>(expr) != nullptr) {
    return true;
  }

  if (!IsInFunction()) {
    // Only consider mutating expressions that occur inside functions.
    return true;
  }

  if (var_decl_source_locations_.contains(expr->getBeginLoc())) {
    // The start of the expression coincides with the source location of a
    // variable declaration. This happens when the expression has the form:
    // "auto v = ...", e.g. occurring in "if (auto v = ...)". Here, the source
    // location for "v" is associated with both the declaration of "v" and the
    // condition expression for the if statement. It must not be mutated,
    // because this would lead to invalid code of the form:
    // "if (auto __dredd_fun(v) = ...)".
    return true;
  }

  if (GetSourceRangeInMainFile(compiler_instance_.getPreprocessor(), *expr)
          .isInvalid()) {
    return true;
  }

  // Check that the result type is supported
  if (!IsTypeSupported(expr->getType())) {
    return true;
  }

  if (auto* unary_operator = llvm::dyn_cast<clang::UnaryOperator>(expr)) {
    HandleUnaryOperator(unary_operator);
  }

  if (auto* binary_operator = llvm::dyn_cast<clang::BinaryOperator>(expr)) {
    HandleBinaryOperator(binary_operator);
  }

  // L-values are only mutated by inserting the prefix operators ++ and --, and
  // only under specific circumstances as documented by
  // MutationReplaceExpr::CanMutateLValue.
  if (expr->isLValue() && !MutationReplaceExpr::CanMutateLValue(
                              compiler_instance_.getASTContext(), *expr)) {
    return true;
  }

  // As it is not possible to pass bit-fields by reference, mutation of
  // bit-field l-values is not supported.
  if (expr->refersToBitField()) {
    return true;
  }

  mutation_tree_path_.back()->AddMutation(
      std::make_unique<MutationReplaceExpr>(*expr));

  return true;
}

bool MutateVisitor::TraverseCompoundStmt(clang::CompoundStmt* compound_stmt) {
  for (auto* stmt : compound_stmt->body()) {
    // To ensure that each sub-statement of a compound statement has its
    // mutations recorded in sibling subtrees of the mutation tree, a mutation
    // tree node is pushed per sub-statement.
    PushMutationTreeRAII push_mutation_tree(*this);
    TraverseStmt(stmt);
    if (GetSourceRangeInMainFile(compiler_instance_.getPreprocessor(), *stmt)
            .isInvalid() ||
        llvm::dyn_cast<clang::NullStmt>(stmt) != nullptr ||
        llvm::dyn_cast<clang::DeclStmt>(stmt) != nullptr ||
        llvm::dyn_cast<clang::SwitchCase>(stmt) != nullptr ||
        llvm::dyn_cast<clang::LabelStmt>(stmt) != nullptr ||
        llvm::dyn_cast<clang::CompoundStmt>(stmt) != nullptr) {
      // Wrapping switch cases, labels and null statements in conditional code
      // has no effect. Declarations cannot be wrapped in conditional code
      // without risking breaking compilation. It is probably redundant to
      // remove a compound statement since each of its sub-statements will be
      // considered for removal anyway.
      continue;
    }
    assert(!enclosing_decls_.empty() &&
           "Statements can only be removed if they are nested in some "
           "declaration.");
    mutation_tree_path_.back()->AddMutation(
        std::make_unique<MutationRemoveStatement>(*stmt));
  }
  return true;
}

bool MutateVisitor::VisitVarDecl(clang::VarDecl* var_decl) {
  var_decl_source_locations_.insert(var_decl->getLocation());
  return true;
}

}  // namespace dredd
