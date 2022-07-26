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

#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Frontend/CompilerInstance.h"
#include "libdredd/mutation_remove_statement.h"
#include "libdredd/mutation_replace_binary_operator.h"
#include "libdredd/util.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/Casting.h"

namespace dredd {

MutateVisitor::MutateVisitor(const clang::CompilerInstance& compiler_instance)
    : compiler_instance_(compiler_instance),
      first_decl_in_source_file_(nullptr) {}

bool MutateVisitor::IsTypeSupported(const clang::BuiltinType* type) {
  return type == nullptr || !(type->isInteger() || type->isFloatingPoint());
}

bool MutateVisitor::TraverseDecl(clang::Decl* decl) {
  if (llvm::dyn_cast<clang::TranslationUnitDecl>(decl) != nullptr) {
    // This is the top-level translation unit declaration, so descend into it.
    return RecursiveASTVisitor::TraverseDecl(decl);
  }
  if (GetSourceRangeInMainFile(compiler_instance_.getPreprocessor(), *decl)
          .isInvalid()) {
    // This declaration is not wholly contained in the main file, so do not
    // consider it for mutation.
    return true;
  }
  if (first_decl_in_source_file_ == nullptr) {
    // This is the first declaration wholly contained in the main file that has
    // been encountered: record it so that the dredd prelude can be inserted
    // before it.
    first_decl_in_source_file_ = decl;
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
  enclosing_decls_.push_back(decl);
  // Consider the declaration for mutation.
  RecursiveASTVisitor::TraverseDecl(decl);
  enclosing_decls_.pop_back();

  return true;
}

bool MutateVisitor::VisitBinaryOperator(
    clang::BinaryOperator* binary_operator) {
  if (enclosing_decls_.empty() || (llvm::dyn_cast<clang::FunctionDecl>(
                                       enclosing_decls_.back()) == nullptr)) {
    // Only consider mutating binary expressions that occur inside functions.
    return true;
  }

  // Check that the binary expression and its arguments have source
  // ranges that are part of the main file. In particular, this avoids
  // mutating expressions that directly involve the use of macros (though it
  // is OK if sub-expressions of arguments use macros).
  if (GetSourceRangeInMainFile(compiler_instance_.getPreprocessor(),
                               *binary_operator)
          .isInvalid() ||
      GetSourceRangeInMainFile(compiler_instance_.getPreprocessor(),
                               *binary_operator->getLHS())
          .isInvalid() ||
      GetSourceRangeInMainFile(compiler_instance_.getPreprocessor(),
                               *binary_operator->getRHS())
          .isInvalid()) {
    return true;
  }

  // We only want to change operators for binary operations on basic types.
  // In particular, we do not want to mess with pointer arithmetic.
  const auto* result_type =
      binary_operator->getType()->getAs<clang::BuiltinType>();
  if (IsTypeSupported(result_type)) {
    return true;
  }
  const auto* lhs_type =
      binary_operator->getLHS()->getType()->getAs<clang::BuiltinType>();
  if (IsTypeSupported(lhs_type)) {
    return true;
  }
  const auto* rhs_type =
      binary_operator->getRHS()->getType()->getAs<clang::BuiltinType>();
  if (IsTypeSupported(rhs_type)) {
    return true;
  }

  if (binary_operator->isCommaOp()) {
    // The comma operator is so versatile that it does not make a great deal of
    // sense to try to rewrite it.
    return true;
  }

  mutations_.push_back(
      std::make_unique<MutationReplaceBinaryOperator>(*binary_operator));
  return true;
}

bool MutateVisitor::VisitCompoundStmt(clang::CompoundStmt* compound_stmt) {
  for (auto* stmt : compound_stmt->body()) {
    if (GetSourceRangeInMainFile(compiler_instance_.getPreprocessor(), *stmt)
            .isInvalid() ||
        llvm::dyn_cast<clang::NullStmt>(stmt) != nullptr ||
        llvm::dyn_cast<clang::DeclStmt>(stmt) != nullptr ||
        llvm::dyn_cast<clang::SwitchCase>(stmt) != nullptr ||
        llvm::dyn_cast<clang::LabelStmt>(stmt) != nullptr) {
      // Wrapping switch cases, labels and null statements in conditional code
      // has no effect. Declarations cannot be wrapped in conditional code
      // without risking breaking compilation.
      continue;
    }
    assert(!enclosing_decls_.empty() &&
           "Statements can only be removed if they are nested in some "
           "declaration.");
    mutations_.push_back(std::make_unique<MutationRemoveStatement>(*stmt));
  }
  return true;
}

}  // namespace dredd
