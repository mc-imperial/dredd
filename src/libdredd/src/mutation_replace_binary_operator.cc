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

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/Expr.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/util.h"
#include "llvm/ADT/StringRef.h"

namespace dredd {

MutationReplaceBinaryOperator::MutationReplaceBinaryOperator(
    const clang::BinaryOperator& binary_operator,
    const clang::Decl& enclosing_decl)
    : binary_operator_(binary_operator), enclosing_decl_(enclosing_decl) {}

std::string MutationReplaceBinaryOperator::GenerateMutatorFunction(
    const std::string& function_name, const std::string& result_type,
    const std::string& lhs_type, const std::string& rhs_type,
    const std::vector<clang::BinaryOperatorKind>& operators,
    int& mutation_id) const {
  std::stringstream new_function;
  new_function << "static " << result_type << " " << function_name << "(";

  std::string arg1_evaluated("arg1()");
  new_function << "std::function<" << lhs_type << "()>"
               << " arg1, ";

  std::string arg2_evaluated("arg2()");
  new_function << "std::function<" << rhs_type << "()>"
               << " arg2) {\n";
  new_function << "  switch (__dredd_enabled_mutation()) {\n";

  // Consider every operator apart from the existing operator
  for (auto op : operators) {
    if (op == binary_operator_.getOpcode()) {
      continue;
    }
    new_function << "    case " << mutation_id << ": return " << arg1_evaluated
                 << " " << clang::BinaryOperator::getOpcodeStr(op).str() << " "
                 << arg2_evaluated << ";\n";
    mutation_id++;
  }
  if (!binary_operator_.isAssignmentOp()) {
    // LHS
    new_function << "    case " << mutation_id << ": return " << arg1_evaluated
                 << ";\n";
    mutation_id++;

    // RHS
    new_function << "    case " << mutation_id << ": return " << arg2_evaluated
                 << ";\n";
    mutation_id++;
  }
  if (binary_operator_.isLogicalOp()) {
    // true
    new_function << "    case " << mutation_id << ": return true;\n";
    mutation_id++;

    // false
    new_function << "    case " << mutation_id << ": return false;\n";
    mutation_id++;
  }

  new_function
      << "    default: return " << arg1_evaluated << " "
      << clang::BinaryOperator::getOpcodeStr(binary_operator_.getOpcode()).str()
      << " " << arg2_evaluated << ";\n";
  new_function << "  }\n";
  new_function << "}\n\n";
  return new_function.str();
}

void MutationReplaceBinaryOperator::Apply(
    clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
    int& mutation_id, clang::Rewriter& rewriter) const {
  // The name of the mutation wrapper function to be used for this
  // replacement.
  std::string mutation_function_name("__dredd_replace_binary_operator_" +
                                     std::to_string(mutation_id));
  std::string result_type = binary_operator_.getType()
                                ->getAs<clang::BuiltinType>()
                                ->getName(ast_context.getPrintingPolicy())
                                .str();
  std::string lhs_type = binary_operator_.getLHS()
                             ->getType()
                             ->getAs<clang::BuiltinType>()
                             ->getName(ast_context.getPrintingPolicy())
                             .str();
  std::string rhs_type = binary_operator_.getRHS()
                             ->getType()
                             ->getAs<clang::BuiltinType>()
                             ->getName(ast_context.getPrintingPolicy())
                             .str();
  if (binary_operator_.isAssignmentOp()) {
    result_type += "&";
    lhs_type += "&";
  }

  std::string new_function;

  std::vector<clang::BinaryOperatorKind> arithmetic_operators = {
      clang::BinaryOperatorKind::BO_Add, clang::BinaryOperatorKind::BO_Div,
      clang::BinaryOperatorKind::BO_Mul, clang::BinaryOperatorKind::BO_Rem,
      clang::BinaryOperatorKind::BO_Sub};

  std::vector<clang::BinaryOperatorKind> assignment_operators = {
      clang::BinaryOperatorKind::BO_AddAssign,
      clang::BinaryOperatorKind::BO_AndAssign,
      clang::BinaryOperatorKind::BO_Assign,
      clang::BinaryOperatorKind::BO_DivAssign,
      clang::BinaryOperatorKind::BO_MulAssign,
      clang::BinaryOperatorKind::BO_OrAssign,
      clang::BinaryOperatorKind::BO_RemAssign,
      clang::BinaryOperatorKind::BO_ShlAssign,
      clang::BinaryOperatorKind::BO_ShrAssign,
      clang::BinaryOperatorKind::BO_SubAssign,
      clang::BinaryOperatorKind::BO_XorAssign};

  std::vector<clang::BinaryOperatorKind> bitwise_operators = {
      clang::BinaryOperatorKind::BO_And, clang::BinaryOperatorKind::BO_Or,
      clang::BinaryOperatorKind::BO_Xor};

  std::vector<clang::BinaryOperatorKind> logical_operators = {
      clang::BinaryOperatorKind::BO_LAnd, clang::BinaryOperatorKind::BO_LOr};

  std::vector<clang::BinaryOperatorKind> relational_operators = {
      clang::BinaryOperatorKind::BO_EQ, clang::BinaryOperatorKind::BO_GE,
      clang::BinaryOperatorKind::BO_GT, clang::BinaryOperatorKind::BO_LE,
      clang::BinaryOperatorKind::BO_LT, clang::BinaryOperatorKind::BO_NE};

  std::vector<clang::BinaryOperatorKind> shift_operators = {
      clang::BinaryOperatorKind::BO_Shl, clang::BinaryOperatorKind::BO_Shr};

  for (const auto& operators :
       {arithmetic_operators, assignment_operators, bitwise_operators,
        logical_operators, relational_operators, shift_operators}) {
    if (std::find(operators.begin(), operators.end(),
                  binary_operator_.getOpcode()) != operators.end()) {
      new_function =
          GenerateMutatorFunction(mutation_function_name, result_type, lhs_type,
                                  rhs_type, operators, mutation_id);
      break;
    }
  }
  assert(!new_function.empty() && "Unsupported opcode.");

  clang::SourceRange binary_operator_source_range_in_main_file =
      GetSourceRangeInMainFile(preprocessor, binary_operator_);
  assert(binary_operator_source_range_in_main_file.isValid() &&
         "Invalid source range.");
  clang::SourceRange lhs_source_range_in_main_file =
      GetSourceRangeInMainFile(preprocessor, *binary_operator_.getLHS());
  assert(lhs_source_range_in_main_file.isValid() && "Invalid source range.");
  clang::SourceRange rhs_source_range_in_main_file =
      GetSourceRangeInMainFile(preprocessor, *binary_operator_.getRHS());
  assert(rhs_source_range_in_main_file.isValid() && "Invalid source range.");

  // Replace the binary operator expression with a call to the wrapper
  // function.
  bool result = rewriter.ReplaceText(
      binary_operator_source_range_in_main_file,
      mutation_function_name + "([&]() -> " + lhs_type +
          " { return static_cast<" + lhs_type + ">(" +
          rewriter.getRewrittenText(lhs_source_range_in_main_file) +
          +"); }, [&]() -> " + rhs_type + " { return static_cast<" + rhs_type +
          ">(" + rewriter.getRewrittenText(rhs_source_range_in_main_file) +
          "); })");
  (void)result;  // Keep release-mode compilers happy.
  assert(!result && "Rewrite failed.\n");

  result = rewriter.InsertTextBefore(
      GetSourceRangeInMainFile(preprocessor, enclosing_decl_).getBegin(),
      new_function);
  (void)result;  // Keep release-mode compilers happy.
  assert(!result && "Rewrite failed.\n");
}

}  // namespace dredd
