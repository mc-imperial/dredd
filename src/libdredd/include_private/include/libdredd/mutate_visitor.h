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

#ifndef DREDD_MUTATE_VISITOR_H
#define DREDD_MUTATE_VISITOR_H

#include <cassert>
#include <utility>
#include <vector>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/DeclOpenMP.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/OpenMPClause.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Redeclarable.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtIterator.h"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/ADT/ArrayRef.h"

namespace dredd {

class MutateVisitor : public clang::RecursiveASTVisitor<MutateVisitor> {
 public:
  explicit MutateVisitor(const clang::CompilerInstance& compiler_instance);

  bool TraverseFunctionDecl(clang::FunctionDecl* function_decl) {
    // Check whether this function declaration is in the main source file. If it
    // is not, do not traverse it. If it is, record that it is the enclosing
    // function while traversing it, so that declarations arising from mutations
    // inside the function can be inserted directly before it.

    const clang::SourceManager& source_manager =
        ast_context_.getSourceManager();
    auto begin_file_id =
        source_manager.getFileID(function_decl->getSourceRange().getBegin());
    auto end_file_id =
        source_manager.getFileID(function_decl->getSourceRange().getEnd());
    auto main_file_id = source_manager.getMainFileID();
    if (begin_file_id != main_file_id || end_file_id != main_file_id) {
      // This indicates either that the function is not in the main file, or
      // that it is inside a macro. Either way, skip it.
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

  bool TraverseBinaryOperator(clang::BinaryOperator* binary_operator) {
    // As a proof of concept, this records opportunities to turn a + node into a
    // - node.

    // In order to ensure that mutation opportunities are presented bottom-up,
    // traverse the AST node before processing it.
    RecursiveASTVisitor<MutateVisitor>::TraverseBinaryOperator(binary_operator);

    // First, check that the binary expression and its arguments have source
    // ranges that are part of the main file. In particular, this avoids
    // mutating expressions that directly involve the use of macros (though it
    // is OK if sub-expressions of arguments use macros).
    const clang::SourceManager& source_manager =
        ast_context_.getSourceManager();
    auto begin_file_id =
        source_manager.getFileID(binary_operator->getSourceRange().getBegin());
    auto end_file_id =
        source_manager.getFileID(binary_operator->getSourceRange().getEnd());

    auto lhs_begin_file_id = source_manager.getFileID(
        binary_operator->getLHS()->getSourceRange().getBegin());
    auto lhs_end_file_id = source_manager.getFileID(
        binary_operator->getLHS()->getSourceRange().getEnd());

    auto rhs_begin_file_id = source_manager.getFileID(
        binary_operator->getRHS()->getSourceRange().getBegin());
    auto rhs_end_file_id = source_manager.getFileID(
        binary_operator->getRHS()->getSourceRange().getEnd());

    auto main_file_id = source_manager.getMainFileID();

    // TODO(afd): It is likely that a lot of checks such as this will be
    // required throughout the code-base, so a helper function would make sense.
    if (!(main_file_id == begin_file_id && main_file_id == end_file_id &&
          main_file_id == lhs_begin_file_id &&
          main_file_id == lhs_end_file_id &&
          main_file_id == rhs_begin_file_id &&
          main_file_id == rhs_end_file_id)) {
      return true;
    }
    // At the moment, restrict attention to +
    if (binary_operator->getOpcode() != clang::BinaryOperatorKind::BO_Add) {
      return true;
    }
    to_replace_.emplace_back(binary_operator, enclosing_function_);
    return true;
  }

  // Return opportunities to replace + with -
  const std::vector<std::pair<clang::BinaryOperator*, clang::FunctionDecl*>>&
  GetReplacements() const {
    return to_replace_;
  }

 private:
  const clang::ASTContext& ast_context_;

  // Tracks the function currently being traversed; nullptr if there is no such
  // function.
  clang::FunctionDecl* enclosing_function_;

  // As a proof of concept this currently stores opportunities for changing a +
  // to a -.
  std::vector<std::pair<clang::BinaryOperator*, clang::FunctionDecl*>>
      to_replace_;
};

}  // namespace dredd

#endif  // DREDD_MUTATE_VISITOR_H
