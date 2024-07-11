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

#include "clang/AST/Attrs.inc"
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
#include "clang/Basic/TypeTraits.h"
#include "clang/Frontend/CompilerInstance.h"
#include "libdredd/mutation.h"
#include "libdredd/mutation_remove_stmt.h"
#include "libdredd/mutation_replace_binary_operator.h"
#include "libdredd/mutation_replace_expr.h"
#include "libdredd/mutation_replace_unary_operator.h"
#include "libdredd/util.h"
#include "llvm/Support/Casting.h"

namespace dredd {

MutateVisitor::MutateVisitor(const clang::CompilerInstance& compiler_instance,
                             bool optimise_mutations)
    : compiler_instance_(&compiler_instance),
      optimise_mutations_(optimise_mutations),
      mutation_tree_root_() {
  mutation_tree_path_.push_back(&mutation_tree_root_);
}

bool MutateVisitor::IsTypeSupported(const clang::QualType qual_type) {
  if (qual_type.isNull()) {
    return false;
  }
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
  if (decl == nullptr) {
    // A Clang AST can feature nodes with null children; e.g.
    // "catch (...)" leads to a null catch expression.
    return true;
  }
  if (llvm::dyn_cast<clang::TranslationUnitDecl>(decl) != nullptr) {
    // This is the top-level translation unit declaration, so descend into it.
    const bool result = RecursiveASTVisitor::TraverseDecl(decl);
    // At this point the translation unit has been fully visited, so the
    // mutation tree that has been built can be made simpler, in preparation for
    // later turning it into a JSON summary.
    mutation_tree_root_.TidyUp();
    return result;
  }
  auto source_range_in_main_file =
      GetSourceRangeInMainFile(compiler_instance_->getPreprocessor(), *decl);
  if (source_range_in_main_file.isInvalid()) {
    // This declaration is not wholly contained in the main file, so do not
    // consider it for mutation.
    return true;
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
  if (const auto* function_decl = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
    if (function_decl->isConstexpr()) {
      // Because Dredd's mutations occur dynamically, they cannot be applied to
      // C++ constexpr functions, which require compile-time evaluation.
      return true;
    }
  }
  if (const auto* var_decl = llvm::dyn_cast<clang::VarDecl>(decl)) {
    if (var_decl->isConstexpr() || var_decl->hasAttr<clang::ConstInitAttr>()) {
      // Because Dredd's mutations occur dynamically, they cannot be applied to
      // C++ constexprs or variables that require constant initialization as
      // these both require compile-time evaluation.
      return true;
    }
    if (!compiler_instance_->getLangOpts().CPlusPlus &&
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
  if (stmt == nullptr) {
    return true;
  }

  // Do not mutate user defined literals.
  // TODO(https://github.com/mc-imperial/dredd/issues/223): Consider supporting
  // them.
  if (llvm::dyn_cast<clang::UserDefinedLiteral>(stmt) != nullptr) {
    return true;
  }

  // Do not mutate under a constant expression, since mutation logic is
  // inherently non-constant.
  if (llvm::dyn_cast<clang::ConstantExpr>(stmt) != nullptr) {
    return true;
  }

  // Do not mutate under a 'noexcept', since it requires a constexpr argument
  // and mutation logic is inherently non-constant.
  if (llvm::dyn_cast<clang::CXXNoexceptExpr>(stmt) != nullptr) {
    return true;
  }

  // Do not mutate under 'sizeof' or 'alignof', as this is guaranteed to yield
  // equivalent mutants. Note that we *do* want to mutate above these
  // expressions, hence we ignore children of such expressions, rather than
  // ignoring the expressions themselves.
  if (const auto* unary_expr_or_type_trait_parent =
          GetFirstParentOfType<clang::UnaryExprOrTypeTraitExpr>(
              *stmt, compiler_instance_->getASTContext())) {
    const auto kind = unary_expr_or_type_trait_parent->getKind();
    if (kind == clang::UETT_SizeOf || kind == clang::UETT_AlignOf) {
      return true;
    }
  }

  // Do not mutate the condition of a constexpr if statement.
  if (const auto* if_stmt = GetFirstParentOfType<clang::IfStmt>(
          *stmt, compiler_instance_->getASTContext())) {
    if (if_stmt->isConstexpr() && if_stmt->getCond() == stmt) {
      return true;
    }
  }

  // Do not mutate the array size expression of C++'s NewExpr.
  // For instance, we do not want to mutate `2` in new `a[2]{3, 4}`,
  // as doing so requires type `a` to have zero-argument constructor.
  if (const auto* cxx_new_expr = GetFirstParentOfType<clang::CXXNewExpr>(
          *stmt, compiler_instance_->getASTContext())) {
    if (cxx_new_expr->getArraySize() == stmt) {
      return true;
    }
  }

  // Add a node to the mutation tree to capture any mutations beneath this
  // statement.
  const PushMutationTreeRAII push_mutation_tree(*this);
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
  if (compiler_instance_->getLangOpts().CPlusPlus) {
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

void MutateVisitor::HandleUnaryOperator(clang::UnaryOperator* unary_operator) {
  // Check that the argument to the unary expression has a source ranges that is
  // part of the main file. In particular, this avoids mutating expressions that
  // directly involve the use of macros (though it is OK if sub-expressions of
  // arguments use macros).
  if (GetSourceRangeInMainFile(compiler_instance_->getPreprocessor(),
                               *unary_operator->getSubExpr())
          .isInvalid()) {
    return;
  }

  // Don't mutate unary plus as this is unlikely to lead to a mutation that
  // differs from inserting a unary operator
  if (unary_operator->getOpcode() == clang::UnaryOperatorKind::UO_Plus) {
    return;
  }

  // Check that the argument type is supported
  if (!IsTypeSupported(unary_operator->getSubExpr()->getType())) {
    return;
  }

  // Mutation functions for ++ and -- operators require their argument to be
  // passed by reference. It is not possible to pass bit-fields by reference,
  // this mutation of these operators when applied to bit-fields is not
  // supported.
  if (unary_operator->isIncrementDecrementOp() &&
      unary_operator->getSubExpr()->refersToBitField()) {
    return;
  }

  if (optimise_mutations_) {
    if (unary_operator->getOpcode() == clang::UO_Minus &&
        (MutationReplaceExpr::ExprIsEquivalentToInt(
             *unary_operator->getSubExpr(), 1,
             compiler_instance_->getASTContext()) ||
         MutationReplaceExpr::ExprIsEquivalentToFloat(
             *unary_operator->getSubExpr(), 1.0,
             compiler_instance_->getASTContext()))) {
      return;
    }

    if (unary_operator->getOpcode() == clang::UO_Not &&
        (MutationReplaceExpr::ExprIsEquivalentToInt(
             *unary_operator->getSubExpr(), 0,
             compiler_instance_->getASTContext()) ||
         MutationReplaceExpr::ExprIsEquivalentToFloat(
             *unary_operator->getSubExpr(), 0.0,
             compiler_instance_->getASTContext()) ||
         MutationReplaceExpr::ExprIsEquivalentToInt(
             *unary_operator->getSubExpr(), 1,
             compiler_instance_->getASTContext()) ||
         MutationReplaceExpr::ExprIsEquivalentToFloat(
             *unary_operator->getSubExpr(), 1.0,
             compiler_instance_->getASTContext()))) {
      return;
    }
  }

  mutation_tree_path_.back()->AddMutation(
      std::make_unique<MutationReplaceUnaryOperator>(
          *unary_operator, compiler_instance_->getPreprocessor(),
          compiler_instance_->getASTContext()));
}

void MutateVisitor::HandleBinaryOperator(
    clang::BinaryOperator* binary_operator) {
  // Check that arguments of the binary expression have source ranges that are
  // part of the main file. In particular, this avoids mutating expressions that
  // directly involve the use of macros (though it is OK if sub-expressions of
  // arguments use macros).
  if (GetSourceRangeInMainFile(compiler_instance_->getPreprocessor(),
                               *binary_operator->getLHS())
          .isInvalid() ||
      GetSourceRangeInMainFile(compiler_instance_->getPreprocessor(),
                               *binary_operator->getRHS())
          .isInvalid()) {
    return;
  }

  // We only want to change operators for binary operations on basic types.
  // Check that the argument types are supported.
  if (!IsTypeSupported(binary_operator->getLHS()->getType())) {
    return;
  }
  if (!IsTypeSupported(binary_operator->getRHS()->getType())) {
    return;
  }

  // Mutation functions for assignment operators (for example +=) require their
  // first argument to be passed by reference. It is not possible to pass
  // bit-fields by reference, this mutation of these operators when applied to a
  // bit-field first argument is not supported.
  if (binary_operator->isAssignmentOp() &&
      binary_operator->getLHS()->refersToBitField()) {
    return;
  }

  if (binary_operator->isCommaOp()) {
    // The comma operator is so versatile that it does not make a great deal of
    // sense to try to rewrite it.
    return;
  }

  // There is no useful way to mutate this expression since it is equivalent to
  // replacement with a constant in all cases.
  if (optimise_mutations_ &&
      (MutationReplaceExpr::ExprIsEquivalentToInt(
           *binary_operator->getLHS(), 0,
           compiler_instance_->getASTContext()) ||
       MutationReplaceExpr::ExprIsEquivalentToFloat(
           *binary_operator->getLHS(), 0.0,
           compiler_instance_->getASTContext())) &&
      (MutationReplaceExpr::ExprIsEquivalentToInt(
           *binary_operator->getRHS(), 1,
           compiler_instance_->getASTContext()) ||
       MutationReplaceExpr::ExprIsEquivalentToFloat(
           *binary_operator->getRHS(), 1.0,
           compiler_instance_->getASTContext()))) {
    return;
  }

  mutation_tree_path_.back()->AddMutation(
      std::make_unique<MutationReplaceBinaryOperator>(
          *binary_operator, compiler_instance_->getPreprocessor(),
          compiler_instance_->getASTContext()));
}

void MutateVisitor::HandleExpr(clang::Expr* expr) {
  // L-values are only mutated by inserting the prefix operators ++ and --, and
  // only under specific circumstances as documented by
  // MutationReplaceExpr::CanMutateLValue.
  if (expr->isLValue() && !MutationReplaceExpr::CanMutateLValue(
                              compiler_instance_->getASTContext(), *expr)) {
    return;
  }

  // It is incorrect to attempt to mutate braced initializer lists.
  if (llvm::dyn_cast<clang::InitListExpr>(expr) != nullptr) {
    return;
  }

  // Avoid mutating null pointer assignments, such as int* x = 0, as mutating
  // these expressions in C++ is either not safe or not useful. This mutation
  // is acceptable in C, but we avoid the mutation for consistency.
  if (expr->isNullPointerConstant(
          compiler_instance_->getASTContext(),
          clang::Expr::NullPointerConstantValueDependence()) !=
      clang::Expr::NPCK_NotNull) {
    if (const auto* cast_parent = GetFirstParentOfType<clang::CastExpr>(
            *expr, compiler_instance_->getASTContext())) {
      if (cast_parent->getType()->isAnyPointerType()) {
        return;
      }
    }
  }

  if (IsConversionOfEnumToConstructor(*expr)) {
    return;
  }

  if (optimise_mutations_) {
    // If an expression is the direct child of a cast expression, do not mutate
    // it unless the cast is an l-value to r-value cast. In an l-value to
    // r-value cast it is worth mutating the expression before and after casting
    // since different mutations will arise. In a cast that does not change l/r-
    // value status, it is highly likely that mutations on the inner cast will
    // have the same effect as on the outer cast. The possible differences
    // arising due to a change of type are unlikely to be all that interesting,
    // and r-value to r-value implicit casts are very common, e.g. occurring
    // whenever a signed literal, such as `1`, is used in an unsigned context.
    if (const auto* cast_parent = GetFirstParentOfType<clang::CastExpr>(
            *expr, compiler_instance_->getASTContext())) {
      if (cast_parent->isLValue() == expr->isLValue()) {
        return;
      }
    }

    // If an expression is the direct child of a compound statement or switch
    // case then don't mutate it, because:
    // - replacing it with a constant has the same effect as deleting it;
    // - if it is an r-value, inserting a unary operator has no effect;
    // - if it is an l-value, inserting an increment operator is fairly well
    // captured by inserting an increment before a future use of the l-value (it
    // is not exactly the same, because the future use could be dynamically
    // reached in different manners compared with this statement).
    if (GetFirstParentOfType<clang::CompoundStmt>(
            *expr, compiler_instance_->getASTContext()) != nullptr ||
        GetFirstParentOfType<clang::SwitchCase>(
            *expr, compiler_instance_->getASTContext()) != nullptr) {
      return;
    }
  }

  mutation_tree_path_.back()->AddMutation(std::make_unique<MutationReplaceExpr>(
      *expr, compiler_instance_->getPreprocessor(),
      compiler_instance_->getASTContext()));
}

bool MutateVisitor::VisitExpr(clang::Expr* expr) {
  if (optimise_mutations_ &&
      llvm::dyn_cast<clang::ParenExpr>(expr) != nullptr) {
    // There is no value in mutating a parentheses expression.
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

  if (GetSourceRangeInMainFile(compiler_instance_->getPreprocessor(), *expr)
          .isInvalid()) {
    return true;
  }

  // Introduced to work around what seems like a bug in Clang, where a source
  // range can end earlier than it starts. See "structured_binding.cc" under
  // single file tests. If the Clang issue is indeed a bug and gets fixed, this
  // check (and the associated function) should be removed.
  if (!SourceRangeConsistencyCheck(expr->getSourceRange(),
                                   compiler_instance_->getASTContext())) {
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

  HandleExpr(expr);

  return true;
}

bool MutateVisitor::TraverseCompoundStmt(clang::CompoundStmt* compound_stmt) {
  for (auto* stmt : compound_stmt->body()) {
    // A switch case (a 'case' or 'default') appears as a component of a
    // compound statement, but it is the statement *after* the 'case' or
    // 'default' label that should be considered for removal. This is because
    // the 'if' check for statement removal must occur after the label, not
    // before it, otherwise a branch to the label would branch straight into the
    // body of the 'if' statement that simulates statement removal. Furthermore,
    // if there are many 'case' or 'default' labels in a row, they are
    // arranged hierarchically in the AST. We therefore traverse any such
    // hierarchy until we reach a statement that is not a switch case, and it
    // is this statement that is considered for removal.
    clang::Stmt* target_stmt = stmt;
    while (auto* switch_case = llvm::dyn_cast<clang::SwitchCase>(target_stmt)) {
      target_stmt = switch_case->getSubStmt();
    }
    if (optimise_mutations_) {
      if (const auto* expr = llvm::dyn_cast<clang::Expr>(target_stmt)) {
        if (!expr->HasSideEffects(compiler_instance_->getASTContext())) {
          // There is no point mutating a side-effect free expression statement.
          continue;
        }
      }
    }

    // To ensure that each sub-statement of a compound statement has its
    // mutations recorded in sibling subtrees of the mutation tree, a mutation
    // tree node is pushed per sub-statement.
    const PushMutationTreeRAII push_mutation_tree(*this);
    TraverseStmt(target_stmt);
    if (GetSourceRangeInMainFile(compiler_instance_->getPreprocessor(),
                                 *target_stmt)
            .isInvalid() ||
        llvm::dyn_cast<clang::NullStmt>(target_stmt) != nullptr ||
        llvm::dyn_cast<clang::DeclStmt>(target_stmt) != nullptr ||
        llvm::dyn_cast<clang::LabelStmt>(target_stmt) != nullptr) {
      // Wrapping switch cases, labels and null statements in conditional code
      // has no effect. Declarations cannot be wrapped in conditional code
      // without risking breaking compilation.
      continue;
    }
    if (optimise_mutations_ &&
        llvm::dyn_cast<clang::CompoundStmt>(target_stmt) != nullptr) {
      // It is likely redundant to remove a compound statement since each of its
      // sub-statements will be considered for removal anyway.
      continue;
    }
    assert(!enclosing_decls_.empty() &&
           "Statements can only be removed if they are nested in some "
           "declaration.");
    mutation_tree_path_.back()->AddMutation(
        std::make_unique<MutationRemoveStmt>(
            *target_stmt, compiler_instance_->getPreprocessor(),
            compiler_instance_->getASTContext()));
  }
  return true;
}

bool MutateVisitor::VisitVarDecl(clang::VarDecl* var_decl) {
  var_decl_source_locations_.insert(var_decl->getLocation());
  return true;
}

bool MutateVisitor::IsConversionOfEnumToConstructor(
    const clang::Expr& expr) const {
  // This avoids a special case identified by
  // https://github.com/mc-imperial/dredd/issues/264. The issue arises when a
  // ternary expression has the form "c ? Foo(...) : enum_constant", where Foo
  // is a class/struct that can be implicitly constructed from an int, and that
  // also has the int operator overloaded.
  // In this case, the enum_constant is implicitly cast to an integer, which
  // then implicitly constructs a Foo instance. If the implicit cast is mutated,
  // the mutated code will lead to the conditional operator's third argument
  // having type int. It is then ambiguous whether the second argument,
  // Foo(...), should have type Foo, or type int, because Foo has the int
  // operator overloaded.

  // Check whether this expression is an implicit cast.
  const auto* implicit_cast_expr =
      llvm::dyn_cast<clang::ImplicitCastExpr>(&expr);
  if (implicit_cast_expr == nullptr) {
    return false;
  }
  // Check whether the parent expression is a C++ constructor.
  if (GetFirstParentOfType<clang::CXXConstructExpr>(
          expr, compiler_instance_->getASTContext()) == nullptr) {
    return false;
  }
  // Check whether there is an enum constant under the implicit cast.
  const auto* decl_ref_expr =
      llvm::dyn_cast<clang::DeclRefExpr>(implicit_cast_expr->getSubExpr());
  if (decl_ref_expr == nullptr) {
    return false;
  }
  return llvm::dyn_cast<clang::EnumConstantDecl>(decl_ref_expr->getDecl()) !=
         nullptr;
}

}  // namespace dredd
