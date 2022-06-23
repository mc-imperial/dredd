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

#include "libdredd/mutate_ast_consumer.h"

#include <cassert>
#include <sstream>
#include <vector>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/mutation.h"

namespace dredd {

void MutateAstConsumer::HandleTranslationUnit(clang::ASTContext& context) {
  (void)generator_;
  if (context.getDiagnostics().hasErrorOccurred()) {
    // There has been an error, so we don't do any processing.
    return;
  }
  visitor_->TraverseDecl(context.getTranslationUnitDecl());
  rewriter_.setSourceMgr(compiler_instance_.getSourceManager(),
                         compiler_instance_.getLangOpts());

  clang::PrintingPolicy printing_policy(compiler_instance_.getLangOpts());

  // At present, all possible replacements are made. This should be changed so
  // that a random subset of replacements are made, of the desired number of
  // mutations. By construction, replacements are processed in a bottom-up
  // fashion. This property should be preserved when the specific replacements
  // are made at random, to avoid attempts to rewrite a child node after its
  // parent has been rewritten.
  for (const auto& replacement : visitor_->GetMutations()) {
    replacement->Apply(mutation_id_, rewriter_, printing_policy);
    mutation_id_++;
  }

  if (visitor_->GetFirstDeclInSourceFile() != nullptr) {
    std::stringstream dredd_prelude;
    dredd_prelude << "#include <cstdlib>\n";
    dredd_prelude << "#include <functional>\n\n";
    dredd_prelude << "static int __dredd_enabled_mutation() {\n";
    dredd_prelude << "  const char* __dredd_environment_variable = "
                     "std::getenv(\"DREDD_ENABLED_MUTATION\");\n";
    dredd_prelude << "  if (__dredd_environment_variable == nullptr) {\n";
    dredd_prelude << "    return -1;\n";
    dredd_prelude << "  }\n";
    dredd_prelude << "  return atoi(__dredd_environment_variable);\n";
    dredd_prelude << "}\n\n";
    dredd_prelude
        << "static void __dredd_remove_statement(std::function<void()> "
           "statement, int mutation_id) {\n"
        << "  if (__dredd_enabled_mutation() == mutation_id) {\n"
        << "    return;\n"
        << "  }\n"
        << "  statement();\n"
        << "}\n\n";

    bool result = rewriter_.InsertTextBefore(
        visitor_->GetFirstDeclInSourceFile()->getBeginLoc(),
        dredd_prelude.str());
    (void)result;  // Keep release-mode compilers happy.
    assert(!result && "Rewrite failed.\n");
  }

  bool result = rewriter_.overwriteChangedFiles();
  (void)result;  // Keep release mode compilers happy
  assert(!result && "Something went wrong emitting rewritten files.");
}

}  // namespace dredd
