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

#include <vector>

#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/DeclOpenMP.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/OpenMPClause.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Redeclarable.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtIterator.h"
#include "clang/AST/Type.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/ADT/ArrayRef.h"

namespace dredd {

class MutateVisitor : public clang::RecursiveASTVisitor<MutateVisitor> {
 public:
  explicit MutateVisitor(const clang::CompilerInstance& compiler_instance);

  bool TraverseBinaryOperator(clang::BinaryOperator* binary_operator) {
    RecursiveASTVisitor<MutateVisitor>::TraverseBinaryOperator(binary_operator);
    return true;
  }

 private:
  const clang::ASTContext& ast_context_;
};

}  // namespace dredd

#endif  // DREDD_MUTATE_VISITOR_H
