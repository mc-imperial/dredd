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
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/mutation.h"
#include "llvm/Support/Casting.h"

namespace dredd {

void MutateAstConsumer::HandleTranslationUnit(clang::ASTContext& context) {
  (void)generator_;
  (void)num_mutations_;
  if (context.getDiagnostics().hasErrorOccurred()) {
    // There has been an error, so we don't do any processing.
    return;
  }
  visitor_->TraverseDecl(context.getTranslationUnitDecl());
  rewriter_.setSourceMgr(compiler_instance_.getSourceManager(),
                         compiler_instance_.getLangOpts());

  clang::PrintingPolicy printing_policy(compiler_instance_.getLangOpts());

  if (visitor_->GetMain() != nullptr) {
    std::stringstream pre_main;
    pre_main << "\nint __dredd_enabled_mutation;\n\n"
             << "#include <cstdlib>\n\n";
    std::stringstream start_of_main;
    start_of_main << "\n  {\n"
                  << "    const char* __dredd_environment_variable = "
                     "std::getenv(\"DREDD_ENABLED_MUTATION\");\n"
                  << "    if (__dredd_environment_variable == nullptr) {\n"
                  << "      __dredd_enabled_mutation = -1;\n"
                  << "    } else {\n"
                  << "      __dredd_enabled_mutation = "
                     "atoi(__dredd_environment_variable);\n"
                  << "    }\n"
                  << "  }\n\n";
    rewriter_.InsertTextBefore(visitor_->GetMain()->getBeginLoc(),
                               pre_main.str());
    const auto* body =
        llvm::cast<clang::CompoundStmt>(visitor_->GetMain()->getBody());
    rewriter_.InsertTextAfterToken(body->getLBracLoc(), start_of_main.str());
  }

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
  bool result = rewriter_.overwriteChangedFiles();
  // Keep release mode compilers happy
  (void)result;
  assert(!result && "Something went wrong emitting rewritten files.");
}

}  // namespace dredd
