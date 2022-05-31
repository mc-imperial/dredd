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

#include <fstream>
#include <vector>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/RewriteBuffer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/mutation.h"

namespace dredd {

void MutateAstConsumer::HandleTranslationUnit(clang::ASTContext& context) {
  (void)generator_;
  (void)num_mutations_;
  (void)output_file_;
  if (context.getDiagnostics().hasErrorOccurred()) {
    // There has been an error, so we don't do any processing.
    return;
  }
  visitor_->TraverseDecl(context.getTranslationUnitDecl());
  rewriter_.setSourceMgr(compiler_instance_.getSourceManager(),
                         compiler_instance_.getLangOpts());

  clang::PrintingPolicy printing_policy(compiler_instance_.getLangOpts());

  // Used to make each mutation wrapper function unique.
  int count = 0;
  // At present, all possible replacements are made. This should be changed so
  // that a random subset of replacements are made, of the desired number of
  // mutations. By construction, replacements are processed in a bottom-up
  // fashion. This property should be preserved when the specific replacements
  // are made at random, to avoid attempts to rewrite a child node after its
  // parent has been rewritten.
  for (auto& replacement : visitor_->GetMutations()) {
    replacement->Apply(count, rewriter_, printing_policy);
    count++;
  }
  // TODO(afd): This fails if no rewrites actually took place, so that should be
  // accounted for.
  const clang::RewriteBuffer* rewrite_buffer = rewriter_.getRewriteBufferFor(
      compiler_instance_.getSourceManager().getMainFileID());
  std::ofstream output_file_stream(output_file_, std::ofstream::out);
  output_file_stream << std::string(rewrite_buffer->begin(),
                                    rewrite_buffer->end());

  output_file_stream.close();
}

}  // namespace dredd
