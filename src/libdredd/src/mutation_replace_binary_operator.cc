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

#include "libdredd/mutation_replace_binary_operator.h"

#include <cassert>
#include <sstream>
#include <string>

#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/ADT/StringRef.h"

namespace dredd {

MutationReplaceBinaryOperator::MutationReplaceBinaryOperator(
    const clang::BinaryOperator& binary_operator,
    const clang::FunctionDecl& enclosing_function,
    clang::BinaryOperatorKind new_operator)
    : binary_operator_(binary_operator),
      enclosing_function_(enclosing_function),
      new_operator_(new_operator) {}

void MutationReplaceBinaryOperator::Apply(
    int mutation_id, clang::Rewriter& rewriter,
    clang::PrintingPolicy& printing_policy) const {
  // The name of the mutation wrapper function to be used for this
  // replacement. Right now add-to-sub is the only mutation supported.
  std::string mutation_function_name("__dredd_replace_binary_operator_" +
                                     std::to_string(mutation_id));

  std::string result_type = binary_operator_.getType()
                                ->getAs<clang::BuiltinType>()
                                ->getName(printing_policy)
                                .str();
  std::string lhs_type = binary_operator_.getLHS()
                             ->getType()
                             ->getAs<clang::BuiltinType>()
                             ->getName(printing_policy)
                             .str();
  std::string rhs_type = binary_operator_.getRHS()
                             ->getType()
                             ->getAs<clang::BuiltinType>()
                             ->getName(printing_policy)
                             .str();

  // Replace the binary operator expression with a call to the wrapper
  // function.
  if (binary_operator_.isLogicalOp()) {
    bool result = rewriter.ReplaceText(
        binary_operator_.getSourceRange(),
        mutation_function_name + "([&]() -> " + lhs_type + " { return " +
            rewriter.getRewrittenText(
                binary_operator_.getLHS()->getSourceRange()) +
            +"; }, [&]() -> " + rhs_type + " { return " +
            rewriter.getRewrittenText(
                binary_operator_.getRHS()->getSourceRange()) +
            "; })");
    (void)result;  // Keep release-mode compilers happy.
    assert(!result && "Rewrite failed.\n");
  } else {
    bool result = rewriter.ReplaceText(
        binary_operator_.getSourceRange(),
        mutation_function_name + "(" +
            rewriter.getRewrittenText(
                binary_operator_.getLHS()->getSourceRange()) +
            +", " +
            rewriter.getRewrittenText(
                binary_operator_.getRHS()->getSourceRange()) +
            ")");
    (void)result;  // Keep release-mode compilers happy.
    assert(!result && "Rewrite failed.\n");
  }

  // Generate the wrapper function declaration and insert it before the
  // enclosing function.
  std::stringstream new_function;
  new_function << result_type << " " << mutation_function_name << "(";

  std::string arg1_evaluated("arg1");
  if (binary_operator_.isLogicalOp()) {
    new_function << "std::function<" << lhs_type << "()>";
    arg1_evaluated += "()";
  } else {
    new_function << lhs_type;
  }
  new_function << " arg1, ";

  std::string arg2_evaluated("arg2");
  if (binary_operator_.isLogicalOp()) {
    new_function << "std::function<" << rhs_type << "()>";
    arg2_evaluated += "()";
  } else {
    new_function << rhs_type;
  }
  new_function
      << " arg2) {\n"
      << "  if (__dredd_enabled_mutation() == " << mutation_id << ") {\n"
      << "    return " + arg1_evaluated << " "
      << clang::BinaryOperator::getOpcodeStr(new_operator_).str() << " "
      << arg2_evaluated << ";\n"
      << "  }\n"
      << "  return " << arg1_evaluated << " "
      << clang::BinaryOperator::getOpcodeStr(binary_operator_.getOpcode()).str()
      << " " << arg2_evaluated << ";\n"
      << "}\n\n";

  bool result = rewriter.InsertTextBefore(
      enclosing_function_.getSourceRange().getBegin(), new_function.str());
  (void)result;  // Keep release-mode compilers happy.
  assert(!result && "Rewrite failed.\n");
}

}  // namespace dredd
