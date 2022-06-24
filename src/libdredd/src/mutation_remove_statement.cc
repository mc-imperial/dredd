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

#include "clang/AST/ASTContext.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/TokenKinds.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/Transformer/SourceCode.h"

namespace dredd {

MutationRemoveStatement::MutationRemoveStatement(const clang::Stmt& statement)
    : statement_(statement) {}

void MutationRemoveStatement::Apply(int mutation_id,
                                    clang::ASTContext& ast_context,
                                    clang::Rewriter& rewriter) const {
  clang::SourceRange source_range =
      clang::tooling::getExtendedRange(statement_, clang::tok::TokenKind::semi,
                                       ast_context)
          .getAsRange();

  bool result = rewriter.ReplaceText(
      source_range,
      "if (__dredd_enabled_mutation() == " + std::to_string(mutation_id) +
          ") { " + rewriter.getRewrittenText(source_range) + " }");
  (void)result;  // Keep release-mode compilers happy.
  assert(!result && "Rewrite failed.\n");
}

}  // namespace dredd
