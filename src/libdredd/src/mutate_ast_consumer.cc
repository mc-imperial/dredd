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
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/Type.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/RewriteBuffer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/ADT/StringRef.h"

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

  // Used to make each mutation wrapper function unique.
  int count = 0;
  // At present, all possible replacements are made. This should be changed so
  // that a random subset of replacements are made, of the desired number of
  // mutations. By construction, replacements are processed in a bottom-up
  // fashion. This property should be preserved when the specific replacements
  // are made at random, to avoid attempts to rewrite a child node after its
  // parent has been rewritten.
  for (auto replacement : visitor_->GetReplacements()) {
    auto* binary_operator = replacement.first;

    // The name of the mutation wrapper function to be used for this
    // replacement. Right now add-to-sub is the only mutation supported.
    std::string mutation_function_name("__dredd_add_to_sub_" +
                                       std::to_string(count));

    // Replace the binary operator expression with a call to the wrapper
    // function.
    rewriter_.ReplaceText(binary_operator->getSourceRange(),
                          mutation_function_name + "(" +
                              rewriter_.getRewrittenText(
                                  binary_operator->getLHS()->getSourceRange()) +
                              +", " +
                              rewriter_.getRewrittenText(
                                  binary_operator->getRHS()->getSourceRange()) +
                              ")");

    // Generate the wrapper function declaration and insert it before the
    // enclosing function.
    std::stringstream new_function;
    const clang::BuiltinType* result_type =
        binary_operator->getType()->getAs<clang::BuiltinType>();
    assert(result_type != nullptr);
    const clang::BuiltinType* lhs_type =
        binary_operator->getLHS()->getType()->getAs<clang::BuiltinType>();
    assert(lhs_type != nullptr);
    const clang::BuiltinType* rhs_type =
        binary_operator->getRHS()->getType()->getAs<clang::BuiltinType>();
    assert(rhs_type != nullptr);
    clang::PrintingPolicy printing_policy(compiler_instance_.getLangOpts());
    new_function << "\n"
                 << result_type->getName(printing_policy).str() << " "
                 << mutation_function_name << "("
                 << lhs_type->getName(printing_policy).str() << " arg1, "
                 << rhs_type->getName(printing_policy).str() << " arg2"
                 << ") {\n"
                 << "  return arg1 - arg2;\n"
                 << "}\n\n";

    rewriter_.InsertTextBefore(replacement.second->getSourceRange().getBegin(),
                               new_function.str());
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
