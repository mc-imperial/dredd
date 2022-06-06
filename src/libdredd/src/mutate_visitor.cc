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

#include <algorithm>
#include <cassert>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/OpenMPClause.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtIterator.h"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceManager.h"
#include "libdredd/mutation_replace_binary_operator.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Casting.h"

namespace dredd {

MutateVisitor::MutateVisitor(const clang::ASTContext& ast_context,
                             RandomGenerator& generator)
    : ast_context_(ast_context),
      generator_(generator),
      first_decl_in_source_file_(nullptr),
      enclosing_function_(nullptr) {}

bool MutateVisitor::TraverseDecl(clang::Decl* decl) {
  if (llvm::dyn_cast<clang::TranslationUnitDecl>(decl) != nullptr) {
    // This is the top-level translation unit declaration, so descend into it.
    return RecursiveASTVisitor::TraverseDecl(decl);
  }
  if (!StartsAndEndsInMainSourceFile(*decl)) {
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
  // Consider the declaration for mutation.
  return RecursiveASTVisitor::TraverseDecl(decl);
}

bool MutateVisitor::TraverseFunctionDecl(clang::FunctionDecl* function_decl) {
  // Check whether this function declaration is in the main source file for the
  // translation unit. If it is not, do not traverse it. If it is, record that
  // it is the enclosing function while traversing it, so that declarations
  // arising from mutations inside the function can be inserted directly before
  // it.
  if (!StartsAndEndsInMainSourceFile(*function_decl)) {
    return true;
  }
  // Traverse the function, recording that it is the enclosing function during
  // traversal.
  assert(enclosing_function_ == nullptr);
  enclosing_function_ = function_decl;
  RecursiveASTVisitor::TraverseFunctionDecl(function_decl);
  assert(enclosing_function_ == function_decl);
  enclosing_function_ = nullptr;
  return true;
}

bool MutateVisitor::TraverseBinaryOperator(
    clang::BinaryOperator* binary_operator) {
  // As a proof of concept, this records opportunities to turn a + node into a
  // - node.

  // In order to ensure that mutation opportunities are presented bottom-up,
  // traverse the AST node before processing it.
  RecursiveASTVisitor<MutateVisitor>::TraverseBinaryOperator(binary_operator);

  // Check that the binary expression and its arguments have source
  // ranges that are part of the main file. In particular, this avoids
  // mutating expressions that directly involve the use of macros (though it
  // is OK if sub-expressions of arguments use macros).
  if (!StartsAndEndsInMainSourceFile(*binary_operator) ||
      !StartsAndEndsInMainSourceFile(*binary_operator->getLHS()) ||
      !StartsAndEndsInMainSourceFile(*binary_operator->getRHS())) {
    return true;
  }

  // We only want to change operators for binary operations on basic types.
  // In particular, we do not want to mess with pointer arithmetic.
  const auto* result_type =
      binary_operator->getType()->getAs<clang::BuiltinType>();
  if (result_type == nullptr || !result_type->isInteger()) {
    return true;
  }
  const auto* lhs_type =
      binary_operator->getLHS()->getType()->getAs<clang::BuiltinType>();
  if (lhs_type == nullptr || !lhs_type->isInteger()) {
    return true;
  }
  const auto* rhs_type =
      binary_operator->getRHS()->getType()->getAs<clang::BuiltinType>();
  if (rhs_type == nullptr || !rhs_type->isInteger()) {
    return true;
  }

  std::vector<clang::BinaryOperatorKind> available_operators = {
      clang::BinaryOperatorKind::BO_Mul,  clang::BinaryOperatorKind::BO_Div,
      clang::BinaryOperatorKind::BO_Rem,  clang::BinaryOperatorKind::BO_Add,
      clang::BinaryOperatorKind::BO_Sub,  clang::BinaryOperatorKind::BO_Shl,
      clang::BinaryOperatorKind::BO_Shr,  clang::BinaryOperatorKind::BO_LT,
      clang::BinaryOperatorKind::BO_GT,   clang::BinaryOperatorKind::BO_LE,
      clang::BinaryOperatorKind::BO_GE,   clang::BinaryOperatorKind::BO_EQ,
      clang::BinaryOperatorKind::BO_NE,   clang::BinaryOperatorKind::BO_And,
      clang::BinaryOperatorKind::BO_Xor,  clang::BinaryOperatorKind::BO_Or,
      clang::BinaryOperatorKind::BO_LAnd, clang::BinaryOperatorKind::BO_LOr};
  if (binary_operator->getLHS()->isLValue()) {
    available_operators.push_back(clang::BinaryOperatorKind::BO_Assign);
    available_operators.push_back(clang::BinaryOperatorKind::BO_MulAssign);
    available_operators.push_back(clang::BinaryOperatorKind::BO_DivAssign);
    available_operators.push_back(clang::BinaryOperatorKind::BO_RemAssign);
    available_operators.push_back(clang::BinaryOperatorKind::BO_AddAssign);
    available_operators.push_back(clang::BinaryOperatorKind::BO_SubAssign);
    available_operators.push_back(clang::BinaryOperatorKind::BO_ShlAssign);
    available_operators.push_back(clang::BinaryOperatorKind::BO_ShrAssign);
    available_operators.push_back(clang::BinaryOperatorKind::BO_AndAssign);
    available_operators.push_back(clang::BinaryOperatorKind::BO_XorAssign);
    available_operators.push_back(clang::BinaryOperatorKind::BO_OrAssign);
  }
  available_operators.erase(std::find(available_operators.begin(),
                                      available_operators.end(),
                                      binary_operator->getOpcode()));

  mutations_.push_back(std::make_unique<MutationReplaceBinaryOperator>(
      *binary_operator, *enclosing_function_,
      generator_.GetRandomElement(available_operators)));
  return true;
}

template <typename HasSourceRange>
bool MutateVisitor::StartsAndEndsInMainSourceFile(
    const HasSourceRange& ast_node) const {
  const clang::SourceManager& source_manager = ast_context_.getSourceManager();
  auto begin_file_id =
      source_manager.getFileID(ast_node.getSourceRange().getBegin());
  auto end_file_id =
      source_manager.getFileID(ast_node.getSourceRange().getEnd());
  auto main_file_id = source_manager.getMainFileID();
  return begin_file_id == main_file_id && end_file_id == main_file_id;
}

}  // namespace dredd
