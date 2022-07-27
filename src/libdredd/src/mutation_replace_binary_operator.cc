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
#include <unordered_set>
#include <vector>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/util.h"
#include "llvm/ADT/StringRef.h"

namespace dredd {

namespace {

// Utility method used to avoid spaces when types, such as 'unsigned int', are
// used in mutation function names.
std::string SpaceToUnderscore(const std::string& input) {
  std::string result(input);
  std::replace(result.begin(), result.end(), ' ', '_');
  return result;
}

}  // namespace

MutationReplaceBinaryOperator::MutationReplaceBinaryOperator(
    const clang::BinaryOperator& binary_operator)
    : binary_operator_(binary_operator) {}

std::string MutationReplaceBinaryOperator::GenerateMutatorFunction(
    const std::string& function_name, const std::string& result_type,
    const std::string& lhs_type, const std::string& rhs_type,
    const std::vector<clang::BinaryOperatorKind>& operators,
    int& mutation_id) const {
  std::stringstream new_function;
  new_function << "static " << result_type << " " << function_name;
  new_function << "(std::function<" << lhs_type << "()>"
               << " arg1, ";

  new_function << "std::function<" << rhs_type << "()>"
               << " arg2, int local_mutation_id) {\n";

  int mutant_offset = 0;

  // Consider every operator apart from the existing operator
  std::string arg1_evaluated("arg1()");
  std::string arg2_evaluated("arg2()");
  for (auto op : operators) {
    if (op == binary_operator_.getOpcode()) {
      continue;
    }
    new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                 << mutant_offset << ")) return " << arg1_evaluated << " "
                 << clang::BinaryOperator::getOpcodeStr(op).str() << " "
                 << arg2_evaluated << ";\n";
    mutant_offset++;
  }
  if (!binary_operator_.isAssignmentOp()) {
    // LHS
    new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                 << mutant_offset << ")) return " << arg1_evaluated << ";\n";
    mutant_offset++;

    // RHS
    new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                 << mutant_offset << ")) return " << arg2_evaluated << ";\n";
    mutant_offset++;
  }
  if (binary_operator_.isLogicalOp()) {
    // true
    new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                 << mutant_offset << ")) return true;\n";
    mutant_offset++;

    // false
    new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                 << mutant_offset << ")) return false;\n";
    mutant_offset++;
  }

  new_function
      << "  return " << arg1_evaluated << " "
      << clang::BinaryOperator::getOpcodeStr(binary_operator_.getOpcode()).str()
      << " " << arg2_evaluated << ";\n";
  new_function << "}\n\n";

  // The function captures |mutant_offset| different mutations, so bump up
  // the mutation id accordingly.
  mutation_id += mutant_offset;

  return new_function.str();
}

void MutationReplaceBinaryOperator::Apply(
    clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
    int first_mutation_id_in_file, int& mutation_id, clang::Rewriter& rewriter,
    std::unordered_set<std::string>& dredd_declarations) const {
  std::string new_function_name = "__dredd_replace_binary_operator_";

  // A string corresponding to the binary operator forms part of the name of the
  // mutation function, to differentiate mutation functions for different
  // operators
  switch (binary_operator_.getOpcode()) {
    case clang::BinaryOperatorKind::BO_Add:
      new_function_name += "Add";
      break;
    case clang::BinaryOperatorKind::BO_Div:
      new_function_name += "Div";
      break;
    case clang::BinaryOperatorKind::BO_Mul:
      new_function_name += "Mul";
      break;
    case clang::BinaryOperatorKind::BO_Rem:
      new_function_name += "Rem";
      break;
    case clang::BinaryOperatorKind::BO_Sub:
      new_function_name += "Sub";
      break;
    case clang::BinaryOperatorKind::BO_AddAssign:
      new_function_name += "AddAssign";
      break;
    case clang::BinaryOperatorKind::BO_AndAssign:
      new_function_name += "AndAssign";
      break;
    case clang::BinaryOperatorKind::BO_Assign:
      new_function_name += "Assign";
      break;
    case clang::BinaryOperatorKind::BO_DivAssign:
      new_function_name += "DivAssign";
      break;
    case clang::BinaryOperatorKind::BO_MulAssign:
      new_function_name += "MulAssign";
      break;
    case clang::BinaryOperatorKind::BO_OrAssign:
      new_function_name += "OrAssign";
      break;
    case clang::BinaryOperatorKind::BO_RemAssign:
      new_function_name += "RemAssign";
      break;
    case clang::BinaryOperatorKind::BO_ShlAssign:
      new_function_name += "ShlAssign";
      break;
    case clang::BinaryOperatorKind::BO_ShrAssign:
      new_function_name += "ShrAssign";
      break;
    case clang::BinaryOperatorKind::BO_SubAssign:
      new_function_name += "SubAssign";
      break;
    case clang::BinaryOperatorKind::BO_XorAssign:
      new_function_name += "XorAssign";
      break;
    case clang::BinaryOperatorKind::BO_And:
      new_function_name += "And";
      break;
    case clang::BinaryOperatorKind::BO_Or:
      new_function_name += "Or";
      break;
    case clang::BinaryOperatorKind::BO_Xor:
      new_function_name += "Xor";
      break;
    case clang::BinaryOperatorKind::BO_LAnd:
      new_function_name += "LAnd";
      break;
    case clang::BinaryOperatorKind::BO_LOr:
      new_function_name += "LOr";
      break;
    case clang::BinaryOperatorKind::BO_EQ:
      new_function_name += "EQ";
      break;
    case clang::BinaryOperatorKind::BO_GE:
      new_function_name += "GE";
      break;
    case clang::BinaryOperatorKind::BO_GT:
      new_function_name += "GT";
      break;
    case clang::BinaryOperatorKind::BO_LE:
      new_function_name += "LE";
      break;
    case clang::BinaryOperatorKind::BO_LT:
      new_function_name += "LT";
      break;
    case clang::BinaryOperatorKind::BO_NE:
      new_function_name += "NE";
      break;
    case clang::BinaryOperatorKind::BO_Shl:
      new_function_name += "Shl";
      break;
    case clang::BinaryOperatorKind::BO_Shr:
      new_function_name += "Shr";
      break;
    default:
      assert(false && "Unsupported opcode");
  }

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

  // To avoid problems of ambiguous function calls, the argument types (ignoring
  // whether they are references or not) are baked into the mutation function
  // name. Some type names have space in them (e.g. 'unsigned int'); such spaces
  // are replaced with underscores.
  new_function_name +=
      "_" + SpaceToUnderscore(lhs_type) + "_" + SpaceToUnderscore(rhs_type);

  if (binary_operator_.isAssignmentOp()) {
    result_type += "&";
    lhs_type += "&";
    clang::QualType qualified_lhs_type = binary_operator_.getLHS()->getType();
    if (qualified_lhs_type.isVolatileQualified()) {
      assert(binary_operator_.getType().isVolatileQualified() &&
             "Expected expression to be volatile-qualified since LHS is.");
      result_type = "volatile " + result_type;
      lhs_type = "volatile " + lhs_type;
    }
  }

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
      new_function_name + "([&]() -> " + lhs_type + " { return static_cast<" +
          lhs_type + ">(" +
          rewriter.getRewrittenText(lhs_source_range_in_main_file) +
          +"); }, [&]() -> " + rhs_type + " { return static_cast<" + rhs_type +
          ">(" + rewriter.getRewrittenText(rhs_source_range_in_main_file) +
          "); }, " + std::to_string(mutation_id - first_mutation_id_in_file) +
          ")");
  (void)result;  // Keep release-mode compilers happy.
  assert(!result && "Rewrite failed.\n");

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

  std::string new_function;
  for (const auto& operators :
       {arithmetic_operators, assignment_operators, bitwise_operators,
        logical_operators, relational_operators, shift_operators}) {
    if (std::find(operators.begin(), operators.end(),
                  binary_operator_.getOpcode()) != operators.end()) {
      new_function =
          GenerateMutatorFunction(new_function_name, result_type, lhs_type,
                                  rhs_type, operators, mutation_id);
      break;
    }
  }
  assert(!new_function.empty() && "Unsupported opcode.");

  // Add the mutation function to the set of Dredd declarations - there may
  // already be a matching function, in which case duplication will be avoided.
  dredd_declarations.insert(new_function);
}

}  // namespace dredd
