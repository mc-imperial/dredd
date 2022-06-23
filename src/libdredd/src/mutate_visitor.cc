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
#include "clang/AST/Expr.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtCXX.h"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceManager.h"
#include "libdredd/mutation_remove_statement.h"
#include "libdredd/mutation_replace_binary_operator.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/Casting.h"

namespace dredd {

MutateVisitor::MutateVisitor(const clang::ASTContext& ast_context,
                             RandomGenerator& generator)
    : ast_context_(ast_context),
      generator_(generator),
      first_decl_in_source_file_(nullptr) {}

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
  enclosing_decls_.push_back(decl);
  // Consider the declaration for mutation.
  RecursiveASTVisitor::TraverseDecl(decl);
  enclosing_decls_.pop_back();

  return true;
}

bool MutateVisitor::VisitBinaryOperator(
    clang::BinaryOperator* binary_operator) {
  if (enclosing_decls_.empty()) {
    // Only consider mutating binary expressions that occur inside functions.
    return true;
  }

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

  if (binary_operator->isCommaOp()) {
    // The comma operator is so versatile that it does not make a great deal of
    // sense to try to rewrite it.
    return true;
  }

  std::vector<clang::BinaryOperatorKind> available_operators;
  if (!binary_operator->isLValue()) {
    available_operators.push_back(clang::BinaryOperatorKind::BO_Mul);
    available_operators.push_back(clang::BinaryOperatorKind::BO_Div);
    available_operators.push_back(clang::BinaryOperatorKind::BO_Rem);
    available_operators.push_back(clang::BinaryOperatorKind::BO_Add);
    available_operators.push_back(clang::BinaryOperatorKind::BO_Sub);
    available_operators.push_back(clang::BinaryOperatorKind::BO_Shl);
    available_operators.push_back(clang::BinaryOperatorKind::BO_Shr);
    available_operators.push_back(clang::BinaryOperatorKind::BO_LT);
    available_operators.push_back(clang::BinaryOperatorKind::BO_GT);
    available_operators.push_back(clang::BinaryOperatorKind::BO_LE);
    available_operators.push_back(clang::BinaryOperatorKind::BO_GE);
    available_operators.push_back(clang::BinaryOperatorKind::BO_EQ);
    available_operators.push_back(clang::BinaryOperatorKind::BO_NE);
    available_operators.push_back(clang::BinaryOperatorKind::BO_And);
    available_operators.push_back(clang::BinaryOperatorKind::BO_Xor);
    available_operators.push_back(clang::BinaryOperatorKind::BO_Or);
    available_operators.push_back(clang::BinaryOperatorKind::BO_LAnd);
    available_operators.push_back(clang::BinaryOperatorKind::BO_LOr);
  } else {
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
  auto current_operator_iterator =
      std::find(available_operators.begin(), available_operators.end(),
                binary_operator->getOpcode());
  assert(current_operator_iterator != available_operators.end() &&
         "Unsupported operator.");
  available_operators.erase(current_operator_iterator);

  mutations_.push_back(std::make_unique<MutationReplaceBinaryOperator>(
      *binary_operator, *enclosing_decls_[0],
      generator_.GetRandomElement(available_operators)));
  return true;
}

bool MutateVisitor::VisitCompoundStmt(clang::CompoundStmt* compound_stmt) {
  for (auto* stmt : compound_stmt->body()) {
    bool must_skip = false;
    if (contains_return_goto_or_label_.contains(stmt)) {
      contains_return_goto_or_label_.insert(compound_stmt);
      must_skip = true;
    }
    if (contains_break_for_enclosing_loop_or_switch_.contains(stmt)) {
      contains_break_for_enclosing_loop_or_switch_.insert(compound_stmt);
      must_skip = true;
    }
    if (contains_continue_for_enclosing_loop_.contains(stmt)) {
      contains_continue_for_enclosing_loop_.insert(compound_stmt);
      must_skip = true;
    }
    if (contains_case_for_enclosing_switch_.contains(stmt)) {
      contains_case_for_enclosing_switch_.insert(compound_stmt);
      must_skip = true;
    }
    if (must_skip || !StartsAndEndsInMainSourceFile(*stmt) ||
        llvm::dyn_cast<clang::NullStmt>(stmt) != nullptr ||
        llvm::dyn_cast<clang::DeclStmt>(stmt) != nullptr) {
      continue;
    }
    assert(!enclosing_decls_.empty() &&
           "Statements can only be removed if they are nested in some "
           "declaration.");
    mutations_.push_back(std::make_unique<MutationRemoveStatement>(*stmt));
  }
  return true;
}

bool MutateVisitor::VisitReturnStmt(clang::ReturnStmt* return_stmt) {
  contains_return_goto_or_label_.insert(return_stmt);
  return true;
}

bool MutateVisitor::VisitBreakStmt(clang::BreakStmt* break_stmt) {
  contains_break_for_enclosing_loop_or_switch_.insert(break_stmt);
  return true;
}

bool MutateVisitor::VisitContinueStmt(clang::ContinueStmt* continue_stmt) {
  contains_continue_for_enclosing_loop_.insert(continue_stmt);
  return true;
}

bool MutateVisitor::VisitGotoStmt(clang::GotoStmt* goto_stmt) {
  contains_return_goto_or_label_.insert(goto_stmt);
  return true;
}

bool MutateVisitor::VisitLabelStmt(clang::LabelStmt* label_stmt) {
  contains_return_goto_or_label_.insert(label_stmt);
  return true;
}

bool MutateVisitor::VisitSwitchCase(clang::SwitchCase* switch_case) {
  contains_case_for_enclosing_switch_.insert(switch_case);
  if (contains_return_goto_or_label_.contains(switch_case->getSubStmt())) {
    contains_return_goto_or_label_.insert(switch_case);
  }
  if (contains_continue_for_enclosing_loop_.contains(
          switch_case->getSubStmt())) {
    contains_continue_for_enclosing_loop_.insert(switch_case);
  }
  if (contains_break_for_enclosing_loop_or_switch_.contains(
          switch_case->getSubStmt())) {
    contains_break_for_enclosing_loop_or_switch_.insert(switch_case);
  }
  return true;
}

bool MutateVisitor::VisitIfStmt(clang::IfStmt* if_stmt) {
  std::vector<clang::Stmt*> body_statements;
  body_statements.push_back(if_stmt->getThen());
  if (if_stmt->getElse() != nullptr) {
    body_statements.push_back(if_stmt->getElse());
  }
  for (auto* body_statement : body_statements) {
    if (contains_return_goto_or_label_.contains(body_statement)) {
      contains_return_goto_or_label_.insert(if_stmt);
    }
    if (contains_case_for_enclosing_switch_.contains(body_statement)) {
      contains_case_for_enclosing_switch_.insert(if_stmt);
    }
    if (contains_break_for_enclosing_loop_or_switch_.contains(body_statement)) {
      contains_break_for_enclosing_loop_or_switch_.insert(if_stmt);
    }
    if (contains_continue_for_enclosing_loop_.contains(body_statement)) {
      contains_continue_for_enclosing_loop_.insert(if_stmt);
    }
  }
  return true;
}

bool MutateVisitor::VisitForStmt(clang::ForStmt* for_stmt) {
  if (contains_return_goto_or_label_.contains(for_stmt->getBody())) {
    contains_return_goto_or_label_.insert(for_stmt);
  }
  if (contains_case_for_enclosing_switch_.contains(for_stmt->getBody())) {
    contains_case_for_enclosing_switch_.insert(for_stmt);
  }
  return true;
}

bool MutateVisitor::VisitWhileStmt(clang::WhileStmt* while_stmt) {
  if (contains_return_goto_or_label_.contains(while_stmt->getBody())) {
    contains_return_goto_or_label_.insert(while_stmt);
  }
  if (contains_case_for_enclosing_switch_.contains(while_stmt->getBody())) {
    contains_case_for_enclosing_switch_.insert(while_stmt);
  }
  return true;
}

bool MutateVisitor::VisitDoStmt(clang::DoStmt* do_stmt) {
  if (contains_return_goto_or_label_.contains(do_stmt->getBody())) {
    contains_return_goto_or_label_.insert(do_stmt);
  }
  if (contains_case_for_enclosing_switch_.contains(do_stmt->getBody())) {
    contains_case_for_enclosing_switch_.insert(do_stmt);
  }
  return true;
}

bool MutateVisitor::VisitCXXForRangeStmt(
    clang::CXXForRangeStmt* cxx_for_range_stmt) {
  if (contains_return_goto_or_label_.contains(cxx_for_range_stmt->getBody())) {
    contains_return_goto_or_label_.insert(cxx_for_range_stmt);
  }
  if (contains_case_for_enclosing_switch_.contains(
          cxx_for_range_stmt->getBody())) {
    contains_case_for_enclosing_switch_.insert(cxx_for_range_stmt);
  }
  return true;
}

bool MutateVisitor::VisitSwitchStmt(clang::SwitchStmt* switch_stmt) {
  if (contains_return_goto_or_label_.contains(switch_stmt->getBody())) {
    contains_return_goto_or_label_.insert(switch_stmt);
  }
  if (contains_continue_for_enclosing_loop_.contains(switch_stmt->getBody())) {
    contains_continue_for_enclosing_loop_.insert(switch_stmt);
  }
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
