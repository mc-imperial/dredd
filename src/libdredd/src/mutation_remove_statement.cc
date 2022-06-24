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

#include "libdredd/mutation_remove_statement.h"

#include <cassert>
#include <string>

#include "clang/AST/Expr.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/Stmt.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/Casting.h"

namespace dredd {

MutationRemoveStatement::MutationRemoveStatement(const clang::Stmt& statement)
    : statement_(statement) {}

void MutationRemoveStatement::Apply(
    int mutation_id, clang::Rewriter& rewriter,
    clang::PrintingPolicy& printing_policy) const {
  (void)printing_policy;  // Not used.

  bool needs_trailing_semicolon =
      llvm::dyn_cast<clang::Expr>(&statement_) != nullptr;

  bool result = rewriter.ReplaceText(
      statement_.getSourceRange(),
      "if (__dredd_enabled_mutation() == " + std::to_string(mutation_id) +
          ") { " + rewriter.getRewrittenText(statement_.getSourceRange()) +
          (needs_trailing_semicolon ? ";" : "") + " }");
  (void)result;  // Keep release-mode compilers happy.
  assert(!result && "Rewrite failed.\n");
}

}  // namespace dredd
