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

#include "libdredd/mutation_replace_eager_binary_operator.h"

#include <cassert>
#include <sstream>
#include <string>

#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/ADT/StringRef.h"

namespace dredd {

MutationReplaceEagerBinaryOperator::MutationReplaceEagerBinaryOperator(
    clang::BinaryOperator* binary_operator,
    clang::FunctionDecl* enclosing_function)
    : binary_operator_(binary_operator),
      enclosing_function_(enclosing_function) {}

void MutationReplaceEagerBinaryOperator::Apply(
    int mutation_id, clang::Rewriter& rewriter,
    clang::PrintingPolicy& printing_policy) const {
  // The name of the mutation wrapper function to be used for this
  // replacement. Right now add-to-sub is the only mutation supported.
  std::string mutation_function_name("__dredd_add_to_sub_" +
                                     std::to_string(mutation_id));

  // Replace the binary operator expression with a call to the wrapper
  // function.
  rewriter.ReplaceText(binary_operator_->getSourceRange(),
                       mutation_function_name + "(" +
                           rewriter.getRewrittenText(
                               binary_operator_->getLHS()->getSourceRange()) +
                           +", " +
                           rewriter.getRewrittenText(
                               binary_operator_->getRHS()->getSourceRange()) +
                           ")");

  // Generate the wrapper function declaration and insert it before the
  // enclosing function.
  std::stringstream new_function;
  const clang::BuiltinType* result_type =
      binary_operator_->getType()->getAs<clang::BuiltinType>();
  assert(result_type != nullptr);
  const clang::BuiltinType* lhs_type =
      binary_operator_->getLHS()->getType()->getAs<clang::BuiltinType>();
  assert(lhs_type != nullptr);
  const clang::BuiltinType* rhs_type =
      binary_operator_->getRHS()->getType()->getAs<clang::BuiltinType>();
  assert(rhs_type != nullptr);
  new_function << "\n"
               << result_type->getName(printing_policy).str() << " "
               << mutation_function_name << "("
               << lhs_type->getName(printing_policy).str() << " arg1, "
               << rhs_type->getName(printing_policy).str() << " arg2"
               << ") {\n"
               << "  return arg1 - arg2;\n"
               << "}\n\n";

  rewriter.InsertTextBefore(enclosing_function_->getSourceRange().getBegin(),
                            new_function.str());
}

}  // namespace dredd
