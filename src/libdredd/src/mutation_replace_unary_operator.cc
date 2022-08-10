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

#include "libdredd/mutation_replace_unary_operator.h"

#include <cassert>
#include <sstream>
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

MutationReplaceUnaryOperator::MutationReplaceUnaryOperator(
    const clang::UnaryOperator& unary_operator)
    : unary_operator_(unary_operator) {}

bool MutationReplaceUnaryOperator::IsPrefix(clang::UnaryOperatorKind op) {
  return op != clang::UO_PostInc && op != clang::UO_PostDec;
}

bool MutationReplaceUnaryOperator::IsValidReplacementOperator(
    clang::UnaryOperatorKind op) const {
  if (!unary_operator_.getSubExpr()->isLValue() &&
      (op == clang::UO_PreInc || op == clang::UO_PreDec ||
       op == clang::UO_PostInc || op == clang::UO_PostDec)) {
    // The increment and decrement operators require an l-value.
    return false;
  }

  if ((unary_operator_.getOpcode() == clang::UO_PostDec ||
       unary_operator_.getOpcode() == clang::UO_PostInc) &&
      (op == clang::UO_PreDec || op == clang::UO_PreInc)) {
    // Do not replace a post-increment/decrement with a pre-increment/decrement;
    // it's unlikely to be interesting.
    return false;
  }

  if ((unary_operator_.getOpcode() == clang::UO_PreDec ||
       unary_operator_.getOpcode() == clang::UO_PreInc) &&
      (op == clang::UO_PostDec || op == clang::UO_PostInc)) {
    // Do not replace a pre-increment/decrement with a pre-increment/decrement;
    // it's unlikely to be interesting.
    return false;
  }

  if (unary_operator_.isLValue() &&
      !(op == clang::UO_PreInc || op == clang::UO_PreDec)) {
    // In C++, only pre-increment/decrement operations yield an l-value.
    return false;
  }

  if (op == clang::UO_Not && unary_operator_.getSubExpr()
                                 ->getType()
                                 ->getAs<clang::BuiltinType>()
                                 ->isFloatingPoint()) {
    return false;
  }

  return true;
}

std::string MutationReplaceUnaryOperator::GetFunctionName(
    clang::ASTContext& ast_context) const {
  std::string result = "__dredd_replace_unary_operator_";

  // A string corresponding to the unary operator forms part of the name of the
  // mutation function, to differentiate mutation functions for different
  // operators
  switch (unary_operator_.getOpcode()) {
    case clang::UnaryOperatorKind::UO_Plus:
      result += "Plus";
      break;
    case clang::UnaryOperatorKind::UO_Minus:
      result += "Minus";
      break;
    case clang::UnaryOperatorKind::UO_Not:
      result += "Not";
      break;
    case clang::UnaryOperatorKind::UO_PreDec:
      result += "PreDec";
      break;
    case clang::UnaryOperatorKind::UO_PostDec:
      result += "PostDec";
      break;
    case clang::UnaryOperatorKind::UO_PreInc:
      result += "PreInc";
      break;
    case clang::UnaryOperatorKind::UO_PostInc:
      result += "PostInc";
      break;
    case clang::UnaryOperatorKind::UO_LNot:
      result += "LNot";
      break;
    default:
      assert(false && "Unsupported opcode");
  }

  if (unary_operator_.getSubExpr()->isLValue()) {
    clang::QualType qualified_type = unary_operator_.getSubExpr()->getType();
    if (qualified_type.isVolatileQualified()) {
      assert(unary_operator_.getSubExpr()->getType().isVolatileQualified() &&
             "Expected expression to be volatile-qualified since subexpression "
             "is.");
      result += "_volatile";
    }
  }

  // To avoid problems of ambiguous function calls, the argument types (ignoring
  // whether they are references or not) are baked into the mutation function
  // name. Some type names have space in them (e.g. 'unsigned int'); such spaces
  // are replaced with underscores.
  result +=
      "_" + SpaceToUnderscore(unary_operator_.getSubExpr()
                                  ->getType()
                                  ->getAs<clang::BuiltinType>()
                                  ->getName(ast_context.getPrintingPolicy())
                                  .str());

  return result;
}

std::string MutationReplaceUnaryOperator::GenerateMutatorFunction(
    clang::ASTContext& ast_context, const std::string& function_name,
    const std::string& result_type, const std::string& input_type,
    int& mutation_id) const {
  std::stringstream new_function;
  new_function << "static " << result_type << " " << function_name << "(";
  if (ast_context.getLangOpts().CPlusPlus) {
    new_function << "std::function<" << input_type << "()>";
  } else {
    new_function << input_type;
  }
  new_function << " arg, int local_mutation_id) {\n";

  std::string arg_evaluated = "arg";
  if (ast_context.getLangOpts().CPlusPlus) {
    arg_evaluated += "()";
  } else {
    if (unary_operator_.isIncrementDecrementOp()) {
      arg_evaluated = "(*" + arg_evaluated + ")";
    }
  }

  // Quickly apply the original operator if no mutant is enabled (which will be
  // the common case).
  new_function << "  if (!__dredd_some_mutation_enabled) return ";
  if (IsPrefix(unary_operator_.getOpcode())) {
    new_function
        << clang::UnaryOperator::getOpcodeStr(unary_operator_.getOpcode()).str()
        << arg_evaluated + ";\n";
  } else {
    new_function
        << arg_evaluated
        << clang::UnaryOperator::getOpcodeStr(unary_operator_.getOpcode()).str()
        << ";\n";
  }

  int mutant_offset = 0;
  std::vector<clang::UnaryOperatorKind> operators = {
      clang::UnaryOperatorKind::UO_PreInc, clang::UnaryOperatorKind::UO_PostInc,
      clang::UnaryOperatorKind::UO_PreDec, clang::UnaryOperatorKind::UO_PostDec,
      clang::UnaryOperatorKind::UO_Not,    clang::UnaryOperatorKind::UO_Minus,
      clang::UnaryOperatorKind::UO_LNot};

  for (const auto op : operators) {
    if (op == unary_operator_.getOpcode() || !IsValidReplacementOperator(op)) {
      continue;
    }
    new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                 << mutant_offset << ")) return ";
    if (IsPrefix(op)) {
      new_function << clang::UnaryOperator::getOpcodeStr(op).str()
                   << arg_evaluated + ";\n";
    } else {
      new_function << arg_evaluated
                   << clang::UnaryOperator::getOpcodeStr(op).str() << ";\n";
    }
    mutant_offset++;
  }
  new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
               << mutant_offset << ")) return " + arg_evaluated + ";\n";
  mutant_offset++;

  if (unary_operator_.getOpcode() == clang::UnaryOperatorKind::UO_LNot) {
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

  new_function << "  return ";
  if (IsPrefix(unary_operator_.getOpcode())) {
    new_function
        << clang::UnaryOperator::getOpcodeStr(unary_operator_.getOpcode()).str()
        << arg_evaluated + ";\n";
  } else {
    new_function
        << arg_evaluated
        << clang::UnaryOperator::getOpcodeStr(unary_operator_.getOpcode()).str()
        << ";\n";
  }
  new_function << "}\n\n";

  // The function captures |mutant_offset| different mutations, so bump up
  // the mutation id accordingly.
  mutation_id += mutant_offset;

  return new_function.str();
}

void MutationReplaceUnaryOperator::ApplyCppTypeModifiers(
    const clang::Expr* expr, std::string& type) {
  if (expr->isLValue()) {
    type += "&";
    clang::QualType qualified_type = expr->getType();
    if (qualified_type.isVolatileQualified()) {
      type = "volatile " + type;
    }
  }
}

void MutationReplaceUnaryOperator::ApplyCTypeModifiers(const clang::Expr* expr,
                                                       std::string& type) {
  if (expr->isLValue()) {
    type += "*";
    clang::QualType qualified_type = expr->getType();
    if (qualified_type.isVolatileQualified()) {
      type = "volatile " + type;
    }
  }
}

void MutationReplaceUnaryOperator::Apply(
    clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
    int first_mutation_id_in_file, int& mutation_id, clang::Rewriter& rewriter,
    std::unordered_set<std::string>& dredd_declarations) const {
  std::string new_function_name = GetFunctionName(ast_context);
  std::string result_type = unary_operator_.getType()
                                ->getAs<clang::BuiltinType>()
                                ->getName(ast_context.getPrintingPolicy())
                                .str();
  std::string input_type = unary_operator_.getSubExpr()
                               ->getType()
                               ->getAs<clang::BuiltinType>()
                               ->getName(ast_context.getPrintingPolicy())
                               .str();

  if (ast_context.getLangOpts().CPlusPlus) {
    ApplyCppTypeModifiers(unary_operator_.getSubExpr(), input_type);
    ApplyCppTypeModifiers(&unary_operator_, result_type);
  } else {
    ApplyCTypeModifiers(unary_operator_.getSubExpr(), input_type);
  }

  clang::SourceRange unary_operator_source_range_in_main_file =
      GetSourceRangeInMainFile(preprocessor, unary_operator_);
  assert(unary_operator_source_range_in_main_file.isValid() &&
         "Invalid source range.");
  clang::SourceRange input_source_range_in_main_file =
      GetSourceRangeInMainFile(preprocessor, *unary_operator_.getSubExpr());
  assert(input_source_range_in_main_file.isValid() && "Invalid source range.");

  // Replace the unary operator expression with a call to the wrapper
  // function.
  //
  // Subtracting |first_mutation_id_in_file| turns the global mutation id,
  // |mutation_id|, into a file-local mutation id.
  const int local_mutation_id = mutation_id - first_mutation_id_in_file;
  if (ast_context.getLangOpts().CPlusPlus) {
    bool result = rewriter.ReplaceText(
        unary_operator_source_range_in_main_file,
        new_function_name + "([&]() -> " + input_type + " { return " +
            // We don't need to static cast constant expressions
            (unary_operator_.getSubExpr()->isCXX11ConstantExpr(ast_context)
                 ? rewriter.getRewrittenText(input_source_range_in_main_file)
                 : ("static_cast<" + input_type + ">(" +
                    rewriter.getRewrittenText(input_source_range_in_main_file) +
                    ")")) +
            "; }" + ", " + std::to_string(local_mutation_id) + ")");
    (void)result;  // Keep release-mode compilers happy.
    assert(!result && "Rewrite failed.\n");
  } else {
    std::string input_arg =
        rewriter.getRewrittenText(input_source_range_in_main_file);
    if (unary_operator_.isIncrementDecrementOp()) {
      input_arg = "&(" + input_arg + ")";
    }
    bool result =
        rewriter.ReplaceText(unary_operator_source_range_in_main_file,
                             new_function_name + "(" + input_arg + ", " +
                                 std::to_string(local_mutation_id) + ")");
    (void)result;  // Keep release-mode compilers happy.
    assert(!result && "Rewrite failed.\n");
  }

  std::string new_function;
  new_function = GenerateMutatorFunction(ast_context, new_function_name,
                                         result_type, input_type, mutation_id);
  assert(!new_function.empty() && "Unsupported opcode.");

  dredd_declarations.insert(new_function);
}

}  // namespace dredd
