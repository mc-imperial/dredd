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

#include "clang/AST/APValue.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/Type.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/util.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/StringRef.h"

namespace dredd {

MutationReplaceBinaryOperator::MutationReplaceBinaryOperator(
    const clang::BinaryOperator& binary_operator)
    : binary_operator_(binary_operator) {}

std::string MutationReplaceBinaryOperator::GetExpr(
    clang::ASTContext& ast_context) const {
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
  std::string result =
      arg1_evaluated + " " +
      clang::BinaryOperator::getOpcodeStr(binary_operator_.getOpcode()).str() +
      " " + arg2_evaluated;
  return result;
}

bool MutationReplaceBinaryOperator::IsRedundantReplacementOperator(
    clang::BinaryOperatorKind op, clang::ASTContext& ast_context) const {
  clang::Expr::EvalResult rhs_eval_result;
  clang::Expr::EvalResult lhs_eval_result;

  bool rhs_is_int =
      binary_operator_.getRHS()->EvaluateAsInt(rhs_eval_result, ast_context);
  bool lhs_is_int =
      binary_operator_.getLHS()->EvaluateAsInt(lhs_eval_result, ast_context);

  // In the case where both operands are 0, the only case that isn't covered
  // by constant replacement is undefined behaviour, this is achieved by /.
  if (rhs_is_int &&
      llvm::APSInt::isSameValue(rhs_eval_result.Val.getInt(),
                                llvm::APSInt::get(0)) &&
      lhs_is_int &&
      llvm::APSInt::isSameValue(lhs_eval_result.Val.getInt(),
                                llvm::APSInt::get(0))) {
    if (op == clang::BO_Div) {
      return false;
    }
  }

  // In the following cases, the replacement is equivalent to either replacement
  // with a constant or argument replacement.
  if (rhs_is_int && llvm::APSInt::isSameValue(rhs_eval_result.Val.getInt(),
                                              llvm::APSInt::get(0))) {
    // When the right operand is 0: +, -, << and >> are all equivalent to
    // replacement with the right operand; * is equivalent to replacement with
    // the constant 0 and % is equivalent to replacement with /.
    if (op == clang::BO_Add || op == clang::BO_Sub || op == clang::BO_Shl ||
        op == clang::BO_Shr || op == clang::BO_Mul || op == clang::BO_Rem) {
      return true;
    }
  } else if (rhs_is_int &&
             llvm::APSInt::isSameValue(rhs_eval_result.Val.getInt(),
                                       llvm::APSInt::get(1))) {
    // When the right operand is 1: * and / are equivalent to replacement by the
    // left operand.
    if (op == clang::BO_Mul || op == clang::BO_Div) {
      return true;
    }
  }

  if (lhs_is_int && llvm::APSInt::isSameValue(lhs_eval_result.Val.getInt(),
                                              llvm::APSInt::get(0))) {
    // When the left operand is 0: *, /, %, << and >> are equivalent to
    // replacement by the constant 0 and + is equivalent to replacement by the
    // right operand.
    if (op == clang::BO_Add || op == clang::BO_Shl || op == clang::BO_Shr ||
        op == clang::BO_Mul || op == clang::BO_Rem || op == clang::BO_Div) {
      return true;
    }
  } else if (lhs_is_int &&
             llvm::APSInt::isSameValue(lhs_eval_result.Val.getInt(),
                                       llvm::APSInt::get(1)) &&
             op == clang::BO_Mul) {
    // When the left operand is 1: * is equivalent to replacement by the right
    // operand.
    return true;
  }
  return false;
}

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

  // In the case that we can optimise out some binary expressions, it is
  // important to change the name of the mutator function to avoid clashes
  // with other versions that apply to the same operator and types but cannot
  // be optimised.
  if (!binary_operator_.isAssignmentOp()) {
    clang::Expr::EvalResult eval_result;
    bool rhs_is_int =
        binary_operator_.getRHS()->EvaluateAsInt(eval_result, ast_context);
    if (rhs_is_int && llvm::APSInt::isSameValue(eval_result.Val.getInt(),
                                                llvm::APSInt::get(0))) {
      result += "_rhs_zero";
    } else if (rhs_is_int && llvm::APSInt::isSameValue(eval_result.Val.getInt(),
                                                       llvm::APSInt::get(1))) {
      result += "_rhs_one";
    } else if (rhs_is_int && llvm::APSInt::isSameValue(eval_result.Val.getInt(),
                                                       llvm::APSInt::get(-1))) {
      result += "_rhs_minus_one";
    }

    bool lhs_is_int =
        binary_operator_.getLHS()->EvaluateAsInt(eval_result, ast_context);
    if (lhs_is_int && llvm::APSInt::isSameValue(eval_result.Val.getInt(),
                                                llvm::APSInt::get(0))) {
      result += "_lhs_zero";
    } else if (lhs_is_int && llvm::APSInt::isSameValue(eval_result.Val.getInt(),
                                                       llvm::APSInt::get(1))) {
      result += "_lhs_one";
    } else if (lhs_is_int && llvm::APSInt::isSameValue(eval_result.Val.getInt(),
                                                       llvm::APSInt::get(-1))) {
      result += "_lhs_minus_one";
    }
  }

  return result;
}

void MutationReplaceBinaryOperator::GenerateArgumentReplacement(
    const std::string& arg1_evaluated, const std::string& arg2_evaluated,
    clang::ASTContext& ast_context, bool optimise_mutations,
    std::stringstream& new_function, int& mutant_offset) const {
  if (!binary_operator_.isAssignmentOp()) {
    // LHS
    // These cases are equivalent to constant replacement with the respective
    // constants
    clang::Expr::EvalResult lhs_eval_result;
    bool lhs_is_int =
        binary_operator_.getLHS()->EvaluateAsInt(lhs_eval_result, ast_context);
    if (!optimise_mutations ||
        !(lhs_is_int && (llvm::APSInt::isSameValue(lhs_eval_result.Val.getInt(),
                                                   llvm::APSInt::get(0)) ||
                         llvm::APSInt::isSameValue(lhs_eval_result.Val.getInt(),
                                                   llvm::APSInt::get(1)) ||
                         llvm::APSInt::isSameValue(lhs_eval_result.Val.getInt(),
                                                   llvm::APSInt::get(-1))))) {
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return " << arg1_evaluated << ";\n";
      mutant_offset++;
    }

    // RHS
    // These cases are equivalent to constant replacement with the respective
    // constants
    clang::Expr::EvalResult rhs_eval_result;
    bool rhs_is_int =
        binary_operator_.getRHS()->EvaluateAsInt(rhs_eval_result, ast_context);
    if (!optimise_mutations ||
        !(rhs_is_int && (llvm::APSInt::isSameValue(rhs_eval_result.Val.getInt(),
                                                   llvm::APSInt::get(0)) ||
                         llvm::APSInt::isSameValue(rhs_eval_result.Val.getInt(),
                                                   llvm::APSInt::get(1)) ||
                         llvm::APSInt::isSameValue(rhs_eval_result.Val.getInt(),
                                                   llvm::APSInt::get(-1))))) {
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return " << arg2_evaluated << ";\n";
      mutant_offset++;
    }
  }
}

void MutationReplaceBinaryOperator::GenerateBinaryOperatorReplacement(
    const std::vector<clang::BinaryOperatorKind>& operators,
    const std::string& arg1_evaluated, const std::string& arg2_evaluated,
    clang::ASTContext& ast_context, bool optimise_mutations,
    std::stringstream& new_function, int& mutant_offset) const {
  for (auto op : operators) {
    if (op == binary_operator_.getOpcode() || !IsValidReplacementOperator(op) ||
        (optimise_mutations &&
         IsRedundantReplacementOperator(op, ast_context))) {
      continue;
    }
    new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                 << mutant_offset << ")) return " << arg1_evaluated << " "
                 << clang::BinaryOperator::getOpcodeStr(op).str() << " "
                 << arg2_evaluated << ";\n";
    mutant_offset++;
  }
}

std::string MutationReplaceBinaryOperator::GenerateMutatorFunction(
    clang::ASTContext& ast_context, const std::string& function_name,
    const std::string& result_type, const std::string& lhs_type,
    const std::string& rhs_type, bool optimise_mutations,
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

  GenerateBinaryOperatorReplacement(operators, arg1_evaluated, arg2_evaluated,
                                    ast_context, optimise_mutations,
                                    new_function, mutant_offset);
  GenerateArgumentReplacement(arg1_evaluated, arg2_evaluated, ast_context,
                              optimise_mutations, new_function, mutant_offset);

  new_function << "  return " << GetExpr(ast_context) << ";\n";
  new_function << "}\n\n";

  // The function captures |mutant_offset| different mutations, so bump up
  // the mutation id accordingly.
  mutation_id += mutant_offset;

  return new_function.str();
}

void MutationReplaceBinaryOperator::Apply(
    clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
    int first_mutation_id_in_file, int& mutation_id, bool optimise_mutations,
    clang::Rewriter& rewriter,
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

  if (!ast_context.getLangOpts().CPlusPlus && binary_operator_.isLogicalOp()) {
    // Logical operators in C require special treatment (see the header file for
    // details). Rather than scattering this special treatment throughout the
    // logic for handling other operators, it is simpler to handle this case
    // separately.
    HandleCLogicalOperator(preprocessor, new_function_name, result_type,
                           lhs_type, rhs_type, first_mutation_id_in_file,
                           mutation_id, rewriter, dredd_declarations);
    return;
  }

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
      new_function = GenerateMutatorFunction(
          ast_context, new_function_name, result_type, lhs_type, rhs_type,
          optimise_mutations, operators, mutation_id);
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

  // Replacement of a binary operator with a function call is simulated by:
  // - Replacing the binary operator symbol with a comma (to separate the
  //   arguments of the function call).
  // - Inserting suitable text before and after each argument to the binary
  //   operator.
  // This is preferable over the (otherwise more intuitive) approach of directly
  // replacing the text for the binary operator node, because the Clang rewriter
  // does not support nested replacements.

  // Replace the operator symbol with ","
  rewriter.ReplaceText(
      binary_operator_.getOperatorLoc(),
      static_cast<unsigned int>(
          clang::BinaryOperator::getOpcodeStr(binary_operator_.getOpcode())
              .size()),
      ",");

  // These record the text that should be inserted before and after the LHS and
  // RHS operands.
  std::string lhs_prefix;
  std::string lhs_suffix;
  std::string rhs_prefix;
  std::string rhs_suffix;

  if (ast_context.getLangOpts().CPlusPlus) {
    lhs_prefix = new_function_name + "([&]() -> " + lhs_type +
                 " { return static_cast<" + lhs_type + ">(";
    lhs_suffix = "); }";
    rhs_prefix =
        "[&]() -> " + rhs_type + " { return static_cast<" + rhs_type + ">(";
    rhs_suffix = "); }, " + std::to_string(local_mutation_id) + ")";
  } else {
    lhs_prefix = new_function_name + "(";
    if (binary_operator_.isAssignmentOp()) {
      lhs_prefix.append("&(");
      lhs_suffix.append(")");
    }
    rhs_suffix = ", " + std::to_string(local_mutation_id) + ")";
  }
  // The prefixes and suffixes are ready, so make the relevant insertions.
  bool result = rewriter.InsertTextBefore(
      lhs_source_range_in_main_file.getBegin(), lhs_prefix);
  assert(!result && "Rewrite failed.\n");
  result = rewriter.InsertTextAfterToken(lhs_source_range_in_main_file.getEnd(),
                                         lhs_suffix);
  assert(!result && "Rewrite failed.\n");
  result = rewriter.InsertTextBefore(rhs_source_range_in_main_file.getBegin(),
                                     rhs_prefix);
  assert(!result && "Rewrite failed.\n");
  result = rewriter.InsertTextAfterToken(rhs_source_range_in_main_file.getEnd(),
                                         rhs_suffix);
  assert(!result && "Rewrite failed.\n");
  (void)result;  // Keep release-mode compilers happy.
}

void MutationReplaceBinaryOperator::HandleCLogicalOperator(
    const clang::Preprocessor& preprocessor,
    const std::string& new_function_prefix, const std::string& result_type,
    const std::string& lhs_type, const std::string& rhs_type,
    int first_mutation_id_in_file, int& mutation_id, clang::Rewriter& rewriter,
    std::unordered_set<std::string>& dredd_declarations) const {
  // A C logical operator "op" is handled by transforming:
  //
  //   a op b
  //
  // to:
  //
  //   __dred_fun_outer(__dredd_fun_lhs(a) op __dredd_fun_rhs(b))
  //
  // These functions collectively handle three cases:
  //
  // - Case 0: swapping the operator, achieved by having all functions negate
  //   their argument.
  //
  // - Case 1: replacing the expression with "a", achieved by having the "outer"
  //   and "lhs" functions do nothing, and the "rhs" function return either 0 or
  //   1, depending on the operator.
  //
  // - Case 2: replacing the expression with "b", achieved by having the "outer"
  //   and "rhs" functions do nothing, and the "lhs" function return either 0 or
  //   1, depending on the operator.

  {
    // Rewrite the LHS of the expression, and introduce the associated function.
    auto source_range_lhs =
        GetSourceRangeInMainFile(preprocessor, *binary_operator_.getLHS());
    const std::string lhs_function_name = new_function_prefix + "_lhs";
    rewriter.InsertTextBefore(source_range_lhs.getBegin(),
                              lhs_function_name + "(");
    rewriter.InsertTextAfterToken(
        source_range_lhs.getEnd(),
        ", " + std::to_string(mutation_id - first_mutation_id_in_file) + ")");

    std::stringstream lhs_function;
    lhs_function << "static " << lhs_type << " " << lhs_function_name << "("
                 << lhs_type << " arg, int local_mutation_id) {\n";
    lhs_function << "  if (!__dredd_some_mutation_enabled) return arg;\n";
    // Case 0: swapping the operator.
    // Replacing && with || is achieved by negating the whole expression, and
    // negating each of the LHS and RHS. The same holds for replacing || with
    // &&. This case handles negating the LHS.
    lhs_function << "  if (__dredd_enabled_mutation(local_mutation_id + 0)) "
                    "return !arg;\n";

    // Case 1: replacing with LHS: no action is needed here.

    // Case 2: replacing with RHS.
    if (binary_operator_.getOpcode() == clang::BinaryOperatorKind::BO_LAnd) {
      // Replacing "a && b" with "b" is achieved by replacing "a" with "1".
      lhs_function << "  if (__dredd_enabled_mutation(local_mutation_id + 2)) "
                      "return 1;\n";
    } else {
      // Replacing "a || b" with "b" is achieved by replacing "a" with "0".
      lhs_function << "  if (__dredd_enabled_mutation(local_mutation_id + 2)) "
                      "return 0;\n";
    }
    lhs_function << "  return arg;\n";
    lhs_function << "}\n";
    dredd_declarations.insert(lhs_function.str());
  }

  {
    // Rewrite the RHS of the expression, and introduce the associated function.
    auto source_range_rhs =
        GetSourceRangeInMainFile(preprocessor, *binary_operator_.getRHS());
    const std::string rhs_function_name = new_function_prefix + "_rhs";
    rewriter.InsertTextBefore(source_range_rhs.getBegin(),
                              rhs_function_name + "(");
    rewriter.InsertTextAfterToken(
        source_range_rhs.getEnd(),
        ", " + std::to_string(mutation_id - first_mutation_id_in_file) + ")");

    std::stringstream rhs_function;
    rhs_function << "static " << rhs_type << " " << rhs_function_name << "("
                 << rhs_type << " arg, int local_mutation_id) {\n";
    rhs_function << "  if (!__dredd_some_mutation_enabled) return arg;\n";
    // Case 0: swapping the operator.
    // Replacing && with || is achieved by negating the whole expression, and
    // negating each of the LHS and RHS. The same holds for replacing || with
    // &&. This case handles negating the RHS.
    rhs_function << "  if (__dredd_enabled_mutation(local_mutation_id + 0)) "
                    "return !arg;\n";

    // Case 1: replacing with LHS.
    if (binary_operator_.getOpcode() == clang::BinaryOperatorKind::BO_LAnd) {
      // Replacing "a && b" with "a" is achieved by replacing "b" with "1".
      rhs_function << "  if (__dredd_enabled_mutation(local_mutation_id + 1)) "
                      "return 1;\n";
    } else {
      // Replacing "a || b" with "a" is achieved by replacing "b" with "0".
      rhs_function << "  if (__dredd_enabled_mutation(local_mutation_id + 1)) "
                      "return 0;\n";
    }

    // Case 2: replacing with RHS: no action is needed here.

    rhs_function << "  return arg;\n";
    rhs_function << "}\n";
    dredd_declarations.insert(rhs_function.str());
  }

  {
    // Rewrite the overall expression, and introduce the associated function.
    auto source_range_binary_operator =
        GetSourceRangeInMainFile(preprocessor, binary_operator_);
    const std::string outer_function_name = new_function_prefix + "_outer";
    rewriter.InsertTextBefore(source_range_binary_operator.getBegin(),
                              outer_function_name + "(");
    rewriter.InsertTextAfterToken(
        source_range_binary_operator.getEnd(),
        ", " + std::to_string(mutation_id - first_mutation_id_in_file) + ")");

    std::stringstream outer_function;
    outer_function << "static " << result_type << " " << outer_function_name
                   << "(" << result_type << " arg, int local_mutation_id) {\n";
    // Case 0: swapping the operator.
    // Replacing && with || is achieved by negating the whole expression, and
    // negating each of the LHS and RHS. The same holds for replacing || with
    // &&. This case handles negating the whole expression.
    outer_function << "  if (__dredd_enabled_mutation(local_mutation_id + 0)) "
                      "return !arg;\n";

    // Case 1: replacing with LHS: no action is needed here.

    // Case 2: replacing with RHS: no action is needed here.

    outer_function << "  return arg;\n";
    outer_function << "}\n";
    dredd_declarations.insert(outer_function.str());
  }

  // The mutation id is increased by 3 due to:
  // - Swapping the operator
  // - Replacing with LHS
  // - Replacing with RHS
  mutation_id += 3;
}

}  // namespace dredd
