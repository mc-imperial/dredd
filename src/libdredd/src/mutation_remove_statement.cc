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
#include <sstream>
#include <string>

#include "clang/AST/DeclBase.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtCXX.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/Casting.h"

namespace dredd {

MutationRemoveStatement::MutationRemoveStatement(
    const clang::Stmt& statement, const clang::Decl& enclosing_decl)
    : statement_(statement), enclosing_decl_(enclosing_decl) {}

void MutationRemoveStatement::Apply(
    int mutation_id, clang::Rewriter& rewriter,
    clang::PrintingPolicy& printing_policy) const {
  (void)printing_policy;  // Not used.

  // The name of the mutation wrapper function to be used for this
  // removal.
  std::string mutation_function_name("__dredd_remove_statement_" +
                                     std::to_string(mutation_id));

  // Generate the wrapper function declaration and insert it before the
  // enclosing function.
  std::stringstream new_function;
  new_function << "void " << mutation_function_name
               << "(std::function<void()> statement) {\n"
               << "  if (__dredd_enabled_mutation() == " << mutation_id
               << ") {\n"
               << "    return;\n"
               << "  }\n"
               << "  statement();\n"
               << "}\n\n";

  bool result = rewriter.InsertTextBefore(
      enclosing_decl_.getSourceRange().getBegin(), new_function.str());
  (void)result;  // Keep release-mode compilers happy.
  assert(!result && "Rewrite failed.\n");

  bool needs_trailing_semicolon =
      llvm::dyn_cast<clang::IfStmt>(&statement_) != nullptr ||
      llvm::dyn_cast<clang::ForStmt>(&statement_) != nullptr ||
      llvm::dyn_cast<clang::WhileStmt>(&statement_) != nullptr ||
      llvm::dyn_cast<clang::DoStmt>(&statement_) != nullptr ||
      llvm::dyn_cast<clang::CXXForRangeStmt>(&statement_) != nullptr ||
      llvm::dyn_cast<clang::SwitchStmt>(&statement_) != nullptr ||
      llvm::dyn_cast<clang::CompoundStmt>(&statement_) != nullptr;

  result = rewriter.ReplaceText(
      statement_.getSourceRange(),
      mutation_function_name + "([&]() -> void { " +
          rewriter.getRewrittenText(statement_.getSourceRange()) + "; })" +
          (needs_trailing_semicolon ? ";" : ""));
  (void)result;  // Keep release-mode compilers happy.
  assert(!result && "Rewrite failed.\n");
}

}  // namespace dredd
