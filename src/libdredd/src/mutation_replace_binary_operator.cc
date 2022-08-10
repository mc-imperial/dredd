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
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/util.h"
#include "llvm/ADT/StringRef.h"

namespace dredd {

MutationReplaceBinaryOperator::MutationReplaceBinaryOperator(
    const clang::BinaryOperator& binary_operator)
    : binary_operator_(binary_operator) {}

// Certain operators such as % are not compatible with floating point numbers,
// this function checks which operators can be applied for each type.
bool MutationReplaceBinaryOperator::IsValidReplacementOperator(
    clang::BinaryOperatorKind op) const {
  const auto* lhs_type =
      binary_operator_.getLHS()->getType()->getAs<clang::BuiltinType>();
  assert((lhs_type->isFloatingPoint() || lhs_type->isInteger()) &&
         "Expected lhs to be either integer or floating-point.");
  const auto* rhs_type =
      binary_operator_.getRHS()->getType()->getAs<clang::BuiltinType>();
  assert((rhs_type->isFloatingPoint() || rhs_type->isInteger()) &&
         "Expected rhs to be either integer or floating-point.");
  return !((lhs_type->isFloatingPoint() || rhs_type->isFloatingPoint()) &&
           (op == clang::BO_Rem || op == clang::BO_AndAssign ||
            op == clang::BO_OrAssign || op == clang::BO_RemAssign ||
            op == clang::BO_ShlAssign || op == clang::BO_ShrAssign ||
            op == clang::BO_XorAssign));
}

std::string MutationReplaceBinaryOperator::GetFunctionName(
    clang::ASTContext& ast_context) const {
  std::string result = "__dredd_replace_binary_operator_";

  // A string corresponding to the binary operator forms part of the name of the
  // mutation function, to differentiate mutation functions for different
  // operators
  switch (binary_operator_.getOpcode()) {
    case clang::BinaryOperatorKind::BO_Add:
      result += "Add";
      break;
    case clang::BinaryOperatorKind::BO_Div:
      result += "Div";
      break;
    case clang::BinaryOperatorKind::BO_Mul:
      result += "Mul";
      break;
    case clang::BinaryOperatorKind::BO_Rem:
      result += "Rem";
      break;
    case clang::BinaryOperatorKind::BO_Sub:
      result += "Sub";
      break;
    case clang::BinaryOperatorKind::BO_AddAssign:
      result += "AddAssign";
      break;
    case clang::BinaryOperatorKind::BO_AndAssign:
      result += "AndAssign";
      break;
    case clang::BinaryOperatorKind::BO_Assign:
      result += "Assign";
      break;
    case clang::BinaryOperatorKind::BO_DivAssign:
      result += "DivAssign";
      break;
    case clang::BinaryOperatorKind::BO_MulAssign:
      result += "MulAssign";
      break;
    case clang::BinaryOperatorKind::BO_OrAssign:
      result += "OrAssign";
      break;
    case clang::BinaryOperatorKind::BO_RemAssign:
      result += "RemAssign";
      break;
    case clang::BinaryOperatorKind::BO_ShlAssign:
      result += "ShlAssign";
      break;
    case clang::BinaryOperatorKind::BO_ShrAssign:
      result += "ShrAssign";
      break;
    case clang::BinaryOperatorKind::BO_SubAssign:
      result += "SubAssign";
      break;
    case clang::BinaryOperatorKind::BO_XorAssign:
      result += "XorAssign";
      break;
    case clang::BinaryOperatorKind::BO_And:
      result += "And";
      break;
    case clang::BinaryOperatorKind::BO_Or:
      result += "Or";
      break;
    case clang::BinaryOperatorKind::BO_Xor:
      result += "Xor";
      break;
    case clang::BinaryOperatorKind::BO_LAnd:
      result += "LAnd";
      break;
    case clang::BinaryOperatorKind::BO_LOr:
      result += "LOr";
      break;
    case clang::BinaryOperatorKind::BO_EQ:
      result += "EQ";
      break;
    case clang::BinaryOperatorKind::BO_GE:
      result += "GE";
      break;
    case clang::BinaryOperatorKind::BO_GT:
      result += "GT";
      break;
    case clang::BinaryOperatorKind::BO_LE:
      result += "LE";
      break;
    case clang::BinaryOperatorKind::BO_LT:
      result += "LT";
      break;
    case clang::BinaryOperatorKind::BO_NE:
      result += "NE";
      break;
    case clang::BinaryOperatorKind::BO_Shl:
      result += "Shl";
      break;
    case clang::BinaryOperatorKind::BO_Shr:
      result += "Shr";
      break;
    default:
      assert(false && "Unsupported opcode");
  }

  std::string lhs_qualifier;

  if (binary_operator_.isAssignmentOp()) {
    clang::QualType qualified_lhs_type = binary_operator_.getLHS()->getType();
    if (qualified_lhs_type.isVolatileQualified()) {
      lhs_qualifier = "volatile ";
    }
  }
  // To avoid problems of ambiguous function calls, the argument types (ignoring
  // whether they are references or not) are baked into the mutation function
  // name. Some type names have space in them (e.g. 'unsigned int'); such spaces
  // are replaced with underscores.
  result +=
      "_" + SpaceToUnderscore(lhs_qualifier +
                              binary_operator_.getLHS()
                                  ->getType()
                                  ->getAs<clang::BuiltinType>()
                                  ->getName(ast_context.getPrintingPolicy())
                                  .str());
  result +=
      "_" + SpaceToUnderscore(binary_operator_.getRHS()
                                  ->getType()
                                  ->getAs<clang::BuiltinType>()
                                  ->getName(ast_context.getPrintingPolicy())
                                  .str());

  return result;
}

std::string MutationReplaceBinaryOperator::GenerateMutatorFunction(
    clang::ASTContext& ast_context, const std::string& function_name,
    const std::string& result_type, const std::string& lhs_type,
    const std::string& rhs_type,
    const std::vector<clang::BinaryOperatorKind>& operators,
    int& mutation_id) const {
  std::stringstream new_function;
  new_function << "static " << result_type << " " << function_name << "(";

  if (ast_context.getLangOpts().CPlusPlus) {
    new_function << "std::function<" << lhs_type << "()>";
  } else {
    new_function << lhs_type;
  }
  new_function << " arg1, ";

  if (ast_context.getLangOpts().CPlusPlus) {
    new_function << "std::function<" << rhs_type << "()>";
  } else {
    new_function << rhs_type;
  }

  new_function << " arg2, int local_mutation_id) {\n";

  int mutant_offset = 0;

  // Consider every operator apart from the existing operator
  std::string arg1_evaluated("arg1");
  std::string arg2_evaluated("arg2");
  if (ast_context.getLangOpts().CPlusPlus) {
    arg1_evaluated += "()";
    arg2_evaluated += "()";
  } else {
    if (binary_operator_.isAssignmentOp()) {
      arg1_evaluated = "(*" + arg1_evaluated + ")";
    }
  }

  // Quickly apply the original operator if no mutant is enabled (which will be
  // the common case).
  new_function
      << "  if (!__dredd_some_mutation_enabled) return " << arg1_evaluated
      << " "
      << clang::BinaryOperator::getOpcodeStr(binary_operator_.getOpcode()).str()
      << " " << arg2_evaluated << ";\n";

  for (auto op : operators) {
    if (op == binary_operator_.getOpcode() || !IsValidReplacementOperator(op)) {
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
                 << mutant_offset << ")) return "
                 << (ast_context.getLangOpts().CPlusPlus ? "true" : "1")
                 << ";\n";
    mutant_offset++;

    // false
    new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                 << mutant_offset << ")) return "
                 << (ast_context.getLangOpts().CPlusPlus ? "false" : "0")
                 << ";\n";
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
  std::string new_function_name = GetFunctionName(ast_context);
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
    if (ast_context.getLangOpts().CPlusPlus) {
      result_type += "&";
      lhs_type += "&";
    } else {
      lhs_type += "*";
    }
    clang::QualType qualified_lhs_type = binary_operator_.getLHS()->getType();
    if (qualified_lhs_type.isVolatileQualified()) {
      lhs_type = "volatile " + lhs_type;
      if (ast_context.getLangOpts().CPlusPlus) {
        assert(binary_operator_.getType().isVolatileQualified() &&
               "Expected expression to be volatile-qualified since LHS is.");
        result_type = "volatile " + result_type;
      }
    }
  }

  ReplaceOperator(lhs_type, rhs_type, new_function_name, ast_context,
                  preprocessor, first_mutation_id_in_file, mutation_id,
                  rewriter);

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
          GenerateMutatorFunction(ast_context, new_function_name, result_type,
                                  lhs_type, rhs_type, operators, mutation_id);
      break;
    }
  }
  assert(!new_function.empty() && "Unsupported opcode.");

  // Add the mutation function to the set of Dredd declarations - there may
  // already be a matching function, in which case duplication will be avoided.
  dredd_declarations.insert(new_function);
}

void MutationReplaceBinaryOperator::ReplaceOperator(
    const std::string& lhs_type, const std::string& rhs_type,
    const std::string& new_function_name, clang::ASTContext& ast_context,
    const clang::Preprocessor& preprocessor, int first_mutation_id_in_file,
    int& mutation_id, clang::Rewriter& rewriter) const {
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
  //
  // Subtracting |first_mutation_id_in_file| turns the global mutation id,
  // |mutation_id|, into a file-local mutation id.
  const int local_mutation_id = mutation_id - first_mutation_id_in_file;
  if (ast_context.getLangOpts().CPlusPlus) {
    bool result = rewriter.ReplaceText(
        binary_operator_source_range_in_main_file,
        new_function_name + "([&]() -> " + lhs_type + " { return static_cast<" +
            lhs_type + ">(" +
            rewriter.getRewrittenText(lhs_source_range_in_main_file) +
            +"); }, [&]() -> " + rhs_type + " { return static_cast<" +
            rhs_type + ">(" +
            rewriter.getRewrittenText(rhs_source_range_in_main_file) +
            "); }, " + std::to_string(local_mutation_id) + ")");
    (void)result;  // Keep release-mode compilers happy.
    assert(!result && "Rewrite failed.\n");
  } else {
    std::string lhs_arg =
        rewriter.getRewrittenText(lhs_source_range_in_main_file);
    if (binary_operator_.isAssignmentOp()) {
      lhs_arg = "&(" + lhs_arg + ")";
    }
    bool result = rewriter.ReplaceText(
        binary_operator_source_range_in_main_file,
        new_function_name + "(" + lhs_arg + ", " +
            rewriter.getRewrittenText(rhs_source_range_in_main_file) + ", " +
            std::to_string(local_mutation_id) + ")");
    (void)result;  // Keep release-mode compilers happy.
    assert(!result && "Rewrite failed.\n");
  }
}

}  // namespace dredd
