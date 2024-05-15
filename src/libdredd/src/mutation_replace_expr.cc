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

#include "libdredd/mutation_replace_expr.h"

#include <cassert>
#include <sstream>

#include "clang/AST/APValue.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTTypeTraits.h"
#include "clang/AST/Expr.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/ParentMapContext.h"
#include "clang/AST/Type.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/util.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"

namespace dredd {
dredd::MutationReplaceExpr::MutationReplaceExpr(
    const clang::Expr& expr, const clang::Preprocessor& preprocessor,
    const clang::ASTContext& ast_context)
    : expr_(&expr),
      info_for_source_range_(GetSourceRangeInMainFile(preprocessor, expr),
                             ast_context) {}
std::string MutationReplaceExpr::GetFunctionName(
    bool optimise_mutations, clang::ASTContext& ast_context) const {
  std::string result = "__dredd_replace_expr_";

  if (expr_->isLValue()) {
    const clang::QualType qualified_type = expr_->getType();
    if (qualified_type.isVolatileQualified()) {
      assert(expr_->getType().isVolatileQualified() &&
             "Expected expression to be volatile-qualified since subexpression "
             "is.");
      result += "volatile_";
    }
  }

  // A string corresponding to the expression forms part of the name of the
  // mutation function, to differentiate mutation functions for different
  // types
  result += SpaceToUnderscore(expr_->getType()
                                  ->getAs<clang::BuiltinType>()
                                  ->getName(ast_context.getPrintingPolicy())
                                  .str());

  if (expr_->isLValue()) {
    result += "_lvalue";
  }

  if (optimise_mutations) {
    AddOptimisationSpecifier(ast_context, result);
  }

  return result;
}

void MutationReplaceExpr::AddOptimisationSpecifier(
    clang::ASTContext& ast_context, std::string& function_name) const {
  clang::Expr::EvalResult unused_eval_result;
  if ((expr_->getType()->isIntegerType() &&
       !expr_->getType()->isBooleanType()) ||
      expr_->getType()->isFloatingType()) {
    if (ExprIsEquivalentToInt(*expr_, 0, ast_context) ||
        ExprIsEquivalentToFloat(*expr_, 0.0, ast_context)) {
      function_name += "_zero";
    } else if (ExprIsEquivalentToInt(*expr_, 1, ast_context) ||
               ExprIsEquivalentToFloat(*expr_, 1.0, ast_context)) {
      function_name += "_one";
    } else if (ExprIsEquivalentToInt(*expr_, -1, ast_context) ||
               ExprIsEquivalentToFloat(*expr_, -1.0, ast_context)) {
      function_name += "_minus_one";
    } else if (EvaluateAsInt(*expr_, ast_context, unused_eval_result)) {
      function_name += "_constant";
    }
  }

  if (expr_->getType()->isBooleanType()) {
    if (ExprIsEquivalentToBool(*expr_, true, ast_context)) {
      function_name += "_true";
    } else if (ExprIsEquivalentToBool(*expr_, false, ast_context)) {
      function_name += "_false";
    }
  }

  if (IsRedundantOperatorInsertionBeforeLogicalOperatorArgument(ast_context)) {
    function_name += "_before_logical_operator_argument";
  }

  if (IsBooleanReplacementRedundantForBinaryOperator(true, ast_context)) {
    function_name += "_omit_true";
  }

  if (IsBooleanReplacementRedundantForBinaryOperator(false, ast_context)) {
    function_name += "_omit_false";
  }
}

std::string MutationReplaceExpr::GetExprMacroName(
    const std::string& operator_name, const bool semantics_preserving_mutation,
    const clang::ASTContext& ast_context) const {
  std::string result = "REPLACE_EXPR_" + operator_name;
  if (!semantics_preserving_mutation && ast_context.getLangOpts().CPlusPlus &&
      expr_->HasSideEffects(ast_context)) {
    result += "_EVALUATED";
  } else if (!ast_context.getLangOpts().CPlusPlus && expr_->isLValue()) {
    result += "_POINTER";
  }
  return result;
}

bool MutationReplaceExpr::ExprIsEquivalentToInt(
    const clang::Expr& expr, int constant,
    const clang::ASTContext& ast_context) {
  clang::Expr::EvalResult int_eval_result;
  if (expr.getType()->isIntegerType() &&
      EvaluateAsInt(expr, ast_context, int_eval_result)) {
    return llvm::APSInt::isSameValue(int_eval_result.Val.getInt(),
                                     llvm::APSInt::get(constant));
  }

  return false;
}

bool MutationReplaceExpr::ExprIsEquivalentToFloat(
    const clang::Expr& expr, double constant,
    const clang::ASTContext& ast_context) {
  llvm::APFloat float_eval_result(static_cast<double>(0));
  if (expr.getType()->isFloatingType() &&
      EvaluateAsFloat(expr, ast_context, float_eval_result)) {
    return float_eval_result.isExactlyValue(constant);
  }

  return false;
}

bool MutationReplaceExpr::ExprIsEquivalentToBool(
    const clang::Expr& expr, bool constant,
    const clang::ASTContext& ast_context) {
  bool bool_eval_result = false;
  if (expr.getType()->isBooleanType() &&
      EvaluateAsBooleanCondition(expr, ast_context, bool_eval_result)) {
    return bool_eval_result == constant;
  }

  return false;
}

bool MutationReplaceExpr::IsRedundantOperatorInsertion(
    clang::ASTContext& ast_context,
    clang::UnaryOperatorKind operator_kind) const {
  if (IsRedundantOperatorInsertionBeforeBinaryExpr(ast_context)) {
    return true;
  }

  if (IsRedundantOperatorInsertionBeforeLogicalOperatorArgument(ast_context)) {
    return true;
  }

  switch (operator_kind) {
    case clang::UO_Minus:
      return IsRedundantUnaryMinusInsertion(ast_context);
    case clang::UO_Not:
      return IsRedundantUnaryNotInsertion(ast_context);
    case clang::UO_LNot:
      return IsRedundantUnaryLogicalNotInsertion(ast_context);
    default:
      assert(false && "Unknown operator.");
      return false;
  }
}

void MutationReplaceExpr::GenerateUnaryOperatorInsertionBeforeLValue(
    const std::string& arg_evaluated, clang::ASTContext& ast_context,
    std::unordered_set<std::string>& dredd_macros,
    bool semantics_preserving_mutation, bool only_track_mutant_coverage,
    int mutation_id_base, std::stringstream& new_function,
    int& mutation_id_offset,
    protobufs::MutationReplaceExpr& protobuf_message) const {
  if (!expr_->isLValue() || !CanMutateLValue(ast_context, *expr_)) {
    return;
  }
  if (!only_track_mutant_coverage) {
    const std::string macro_name =
        GetExprMacroName("INC", semantics_preserving_mutation, ast_context);
    new_function << "  " << macro_name << "(" << mutation_id_offset << ");\n";
    dredd_macros.insert(GenerateMutationMacro(macro_name,
                                              semantics_preserving_mutation
                                                  ? arg_evaluated + " + 1"
                                                  : "++(" + arg_evaluated + ")",
                                              semantics_preserving_mutation));
  }
  AddMutationInstance(mutation_id_base,
                      protobufs::MutationReplaceExprAction::InsertPreInc,
                      mutation_id_offset, protobuf_message);

  if (!only_track_mutant_coverage) {
    const std::string macro_name =
        GetExprMacroName("DEC", semantics_preserving_mutation, ast_context);
    new_function << "  " << macro_name << "(" << mutation_id_offset << ");\n";
    dredd_macros.insert(GenerateMutationMacro(macro_name,
                                              semantics_preserving_mutation
                                                  ? arg_evaluated + " - 1"
                                                  : "--(" + arg_evaluated + ")",
                                              semantics_preserving_mutation));
  }
  AddMutationInstance(mutation_id_base,
                      protobufs::MutationReplaceExprAction::InsertPreDec,
                      mutation_id_offset, protobuf_message);
}

void MutationReplaceExpr::GenerateUnaryOperatorInsertionBeforeNonLValue(
    const std::string& arg_evaluated, clang::ASTContext& ast_context,
    std::unordered_set<std::string>& dredd_macros, bool optimise_mutations,
    bool semantics_preserving_mutation, bool only_track_mutant_coverage,
    int mutation_id_base, std::stringstream& new_function,
    int& mutation_id_offset,
    protobufs::MutationReplaceExpr& protobuf_message) const {
  if (expr_->isLValue()) {
    return;
  }
  const clang::BuiltinType& exprType =
      *expr_->getType()->getAs<clang::BuiltinType>();
  // Insert '!'
  if (exprType.isBooleanType() || exprType.isInteger()) {
    if (!optimise_mutations ||
        !IsRedundantOperatorInsertion(ast_context, clang::UO_LNot)) {
      if (!only_track_mutant_coverage) {
        const std::string macro_name = GetExprMacroName(
            "LNOT", semantics_preserving_mutation, ast_context);
        new_function << "  " << macro_name << "(" << mutation_id_offset
                     << ");\n";
        dredd_macros.insert(
            GenerateMutationMacro(macro_name, "!(" + arg_evaluated + ")",
                                  semantics_preserving_mutation));
      }
      AddMutationInstance(mutation_id_base,
                          protobufs::MutationReplaceExprAction::InsertLNot,
                          mutation_id_offset, protobuf_message);
    }
  }

  // Insert '~'
  if (exprType.isInteger() && !exprType.isBooleanType()) {
    if (!optimise_mutations ||
        !IsRedundantOperatorInsertion(ast_context, clang::UO_Not)) {
      if (!only_track_mutant_coverage) {
        const std::string macro_name =
            GetExprMacroName("NOT", semantics_preserving_mutation, ast_context);
        new_function << "  " << macro_name << "(" << mutation_id_offset
                     << ");\n";
        dredd_macros.insert(
            GenerateMutationMacro(macro_name, "~(" + arg_evaluated + ")",
                                  semantics_preserving_mutation));
      }
      AddMutationInstance(mutation_id_base,
                          protobufs::MutationReplaceExprAction::InsertNot,
                          mutation_id_offset, protobuf_message);
    }
  }

  // Insert '-'
  if (exprType.isSignedInteger() || exprType.isFloatingPoint()) {
    if (!optimise_mutations ||
        !IsRedundantOperatorInsertion(ast_context, clang::UO_Minus)) {
      if (!only_track_mutant_coverage) {
        const std::string macro_name = GetExprMacroName(
            "MINUS", semantics_preserving_mutation, ast_context);
        new_function << "  " << macro_name << "(" << mutation_id_offset
                     << ");\n";
        dredd_macros.insert(
            GenerateMutationMacro(macro_name, "-(" + arg_evaluated + ")",
                                  semantics_preserving_mutation));
      }
      AddMutationInstance(mutation_id_base,
                          protobufs::MutationReplaceExprAction::InsertMinus,
                          mutation_id_offset, protobuf_message);
    }
  }
}

void MutationReplaceExpr::GenerateUnaryOperatorInsertion(
    const std::string& arg_evaluated, clang::ASTContext& ast_context,
    std::unordered_set<std::string>& dredd_macros, bool optimise_mutations,
    bool semantics_preserving_mutation, bool only_track_mutant_coverage,
    int mutation_id_base, std::stringstream& new_function,
    int& mutation_id_offset,
    protobufs::MutationReplaceExpr& protobuf_message) const {
  GenerateUnaryOperatorInsertionBeforeLValue(
      arg_evaluated, ast_context, dredd_macros, semantics_preserving_mutation,
      only_track_mutant_coverage, mutation_id_base, new_function,
      mutation_id_offset, protobuf_message);
  GenerateUnaryOperatorInsertionBeforeNonLValue(
      arg_evaluated, ast_context, dredd_macros, optimise_mutations,
      semantics_preserving_mutation, only_track_mutant_coverage,
      mutation_id_base, new_function, mutation_id_offset, protobuf_message);
}

void MutationReplaceExpr::GenerateConstantReplacement(
    clang::ASTContext& ast_context,
    std::unordered_set<std::string>& dredd_macros, bool optimise_mutations,
    bool semantics_preserving_mutation, bool only_track_mutant_coverage,
    int mutation_id_base, std::stringstream& new_function,
    int& mutation_id_offset,
    protobufs::MutationReplaceExpr& protobuf_message) const {
  if (!expr_->isLValue()) {
    GenerateBooleanConstantReplacement(
        ast_context, dredd_macros, optimise_mutations,
        semantics_preserving_mutation, only_track_mutant_coverage,
        mutation_id_base, new_function, mutation_id_offset, protobuf_message);
    GenerateIntegerConstantReplacement(
        ast_context, dredd_macros, optimise_mutations,
        semantics_preserving_mutation, only_track_mutant_coverage,
        mutation_id_base, new_function, mutation_id_offset, protobuf_message);
    GenerateFloatConstantReplacement(
        ast_context, dredd_macros, optimise_mutations,
        semantics_preserving_mutation, only_track_mutant_coverage,
        mutation_id_base, new_function, mutation_id_offset, protobuf_message);
  }
}

void MutationReplaceExpr::GenerateFloatConstantReplacement(
    const clang::ASTContext& ast_context,
    std::unordered_set<std::string>& dredd_macros, bool optimise_mutations,
    bool semantics_preserving_mutation, bool only_track_mutant_coverage,
    int mutation_id_base, std::stringstream& new_function,
    int& mutation_id_offset,
    protobufs::MutationReplaceExpr& protobuf_message) const {
  const clang::BuiltinType& exprType =
      *expr_->getType()->getAs<clang::BuiltinType>();
  if (exprType.isFloatingPoint()) {
    if (!optimise_mutations ||
        !ExprIsEquivalentToFloat(*expr_, 0.0, ast_context)) {
      // Replace floating point expression with 0.0
      if (!only_track_mutant_coverage) {
        const std::string macro_name = "REPLACE_EXPR_FLOAT_ZERO";
        new_function << "  " << macro_name << "(" << mutation_id_offset
                     << ");\n";
        dredd_macros.insert(GenerateMutationMacro(
            macro_name, "0.0", semantics_preserving_mutation));
      }
      AddMutationInstance(
          mutation_id_base,
          protobufs::MutationReplaceExprAction::ReplaceWithZeroFloat,
          mutation_id_offset, protobuf_message);
    }

    if (!optimise_mutations ||
        !ExprIsEquivalentToFloat(*expr_, 1.0, ast_context)) {
      // Replace floating point expression with 1.0
      if (!only_track_mutant_coverage) {
        const std::string macro_name = "REPLACE_EXPR_FLOAT_ONE";
        new_function << "  " << macro_name << "(" << mutation_id_offset
                     << ");\n";
        dredd_macros.insert(GenerateMutationMacro(
            macro_name, "1.0", semantics_preserving_mutation));
      }
      AddMutationInstance(
          mutation_id_base,
          protobufs::MutationReplaceExprAction::ReplaceWithOneFloat,
          mutation_id_offset, protobuf_message);
    }

    if (!optimise_mutations ||
        !ExprIsEquivalentToFloat(*expr_, -1.0, ast_context)) {
      // Replace floating point expression with -1.0
      if (!only_track_mutant_coverage) {
        const std::string macro_name = "REPLACE_EXPR_FLOAT_MINUS_ONE";
        new_function << "  " << macro_name << "(" << mutation_id_offset
                     << ");\n";
        dredd_macros.insert(GenerateMutationMacro(
            macro_name, "-1.0", semantics_preserving_mutation));
      }
      AddMutationInstance(
          mutation_id_base,
          protobufs::MutationReplaceExprAction::ReplaceWithMinusOneFloat,
          mutation_id_offset, protobuf_message);
    }
  }
}
void MutationReplaceExpr::GenerateIntegerConstantReplacement(
    const clang::ASTContext& ast_context,
    std::unordered_set<std::string>& dredd_macros, bool optimise_mutations,
    bool semantics_preserving_mutation, bool only_track_mutant_coverage,
    int mutation_id_base, std::stringstream& new_function,
    int& mutation_id_offset,
    protobufs::MutationReplaceExpr& protobuf_message) const {
  const clang::BuiltinType& exprType =
      *expr_->getType()->getAs<clang::BuiltinType>();
  if (exprType.isInteger() && !exprType.isBooleanType()) {
    if (!optimise_mutations || !ExprIsEquivalentToInt(*expr_, 0, ast_context)) {
      // Replace expression with 0
      if (!only_track_mutant_coverage) {
        const std::string macro_name = "REPLACE_EXPR_INT_ZERO";
        new_function << "  " << macro_name << "(" << mutation_id_offset
                     << ");\n";
        dredd_macros.insert(GenerateMutationMacro(
            macro_name, "0", semantics_preserving_mutation));
      }
      AddMutationInstance(
          mutation_id_base,
          protobufs::MutationReplaceExprAction::ReplaceWithZeroInt,
          mutation_id_offset, protobuf_message);
    }

    if (!optimise_mutations || !ExprIsEquivalentToInt(*expr_, 1, ast_context)) {
      // Replace expression with 1
      if (!only_track_mutant_coverage) {
        const std::string macro_name = "REPLACE_EXPR_INT_ONE";
        new_function << "  " << macro_name << "(" << mutation_id_offset
                     << ");\n";
        dredd_macros.insert(GenerateMutationMacro(
            macro_name, "1", semantics_preserving_mutation));
      }
      AddMutationInstance(
          mutation_id_base,
          protobufs::MutationReplaceExprAction::ReplaceWithOneInt,
          mutation_id_offset, protobuf_message);
    }
  }

  if (exprType.isSignedInteger()) {
    if (!optimise_mutations ||
        !ExprIsEquivalentToInt(*expr_, -1, ast_context)) {
      // Replace signed integer expression with -1
      if (!only_track_mutant_coverage) {
        const std::string macro_name = "REPLACE_EXPR_INT_MINUS_ONE";
        new_function << "  " << macro_name << "(" << mutation_id_offset
                     << ");\n";
        dredd_macros.insert(GenerateMutationMacro(
            macro_name, "-1", semantics_preserving_mutation));
      }
      AddMutationInstance(
          mutation_id_base,
          protobufs::MutationReplaceExprAction::ReplaceWithMinusOneInt,
          mutation_id_offset, protobuf_message);
    }
  }
}
void MutationReplaceExpr::GenerateBooleanConstantReplacement(
    clang::ASTContext& ast_context,
    std::unordered_set<std::string>& dredd_macros, bool optimise_mutations,
    bool semantics_preserving_mutation, bool only_track_mutant_coverage,
    int mutation_id_base, std::stringstream& new_function,
    int& mutation_id_offset,
    protobufs::MutationReplaceExpr& protobuf_message) const {
  const clang::BuiltinType& exprType =
      *expr_->getType()->getAs<clang::BuiltinType>();
  if (exprType.isBooleanType()) {
    if (!optimise_mutations ||
        (!ExprIsEquivalentToBool(*expr_, true, ast_context) &&
         !IsBooleanReplacementRedundantForBinaryOperator(true, ast_context))) {
      // Replace expression with true
      if (!only_track_mutant_coverage) {
        const std::string macro_name = "REPLACE_EXPR_TRUE";
        new_function << "  " << macro_name << "(" << mutation_id_offset
                     << ");\n";
        dredd_macros.insert(GenerateMutationMacro(
            macro_name, (ast_context.getLangOpts().CPlusPlus ? "true" : "1"),
            semantics_preserving_mutation));
      }
      AddMutationInstance(mutation_id_base,
                          protobufs::MutationReplaceExprAction::ReplaceWithTrue,
                          mutation_id_offset, protobuf_message);
    }

    if (!optimise_mutations ||
        (!ExprIsEquivalentToBool(*expr_, false, ast_context) &&
         !IsBooleanReplacementRedundantForBinaryOperator(false, ast_context))) {
      // Replace expression with false
      if (!only_track_mutant_coverage) {
        const std::string macro_name = "REPLACE_EXPR_FALSE";
        new_function << "  " << macro_name << "(" << mutation_id_offset
                     << ");\n";
        dredd_macros.insert(GenerateMutationMacro(
            macro_name, (ast_context.getLangOpts().CPlusPlus ? "false" : "0"),
            semantics_preserving_mutation));
      }
      AddMutationInstance(
          mutation_id_base,
          protobufs::MutationReplaceExprAction::ReplaceWithFalse,
          mutation_id_offset, protobuf_message);
    }
  }
}

std::string MutationReplaceExpr::GenerateMutatorFunction(
    clang::ASTContext& ast_context,
    std::unordered_set<std::string>& dredd_macros,
    const std::string& function_name, const std::string& result_type,
    const std::string& input_type, bool optimise_mutations,
    bool semantics_preserving_mutation, bool only_track_mutant_coverage,
    int& mutation_id, protobufs::MutationReplaceExpr& protobuf_message) const {
  std::stringstream new_function;
  new_function << "static " << result_type << " " << function_name << "(";
  if (!semantics_preserving_mutation && ast_context.getLangOpts().CPlusPlus &&
      expr_->HasSideEffects(ast_context)) {
    new_function << "std::function<" << input_type << "()>";
  } else {
    new_function << input_type;
  }
  new_function << " arg, int local_mutation_id) {\n";

  std::string arg_evaluated = "arg";
  if (!semantics_preserving_mutation && ast_context.getLangOpts().CPlusPlus &&
      expr_->HasSideEffects(ast_context)) {
    arg_evaluated += "()";
  }

  if (!ast_context.getLangOpts().CPlusPlus && expr_->isLValue()) {
    arg_evaluated = "(*" + arg_evaluated + ")";
  }

  if (!only_track_mutant_coverage) {
    // Quickly apply the original operator if no mutant is enabled (which will
    // be the common case).
    new_function << "  MUTATION_PRELUDE(" << arg_evaluated;

    if (semantics_preserving_mutation) {
      new_function << "," << result_type;
    }

    new_function << ");\n";
  }

  int mutation_id_offset = 0;

  GenerateUnaryOperatorInsertion(
      arg_evaluated, ast_context, dredd_macros, optimise_mutations,
      semantics_preserving_mutation, only_track_mutant_coverage, mutation_id,
      new_function, mutation_id_offset, protobuf_message);
  GenerateConstantReplacement(
      ast_context, dredd_macros, optimise_mutations,
      semantics_preserving_mutation, only_track_mutant_coverage, mutation_id,
      new_function, mutation_id_offset, protobuf_message);

  if (only_track_mutant_coverage) {
    new_function << "  __dredd_record_covered_mutants(local_mutation_id, " +
                        std::to_string(mutation_id_offset) + ");\n";
  }

  //  new_function << "  return " << arg_evaluated << ";\n";
  new_function << "  return MUTATION_RETURN(" << arg_evaluated << ");\n";
  new_function << "}\n\n";

  mutation_id += mutation_id_offset;

  return new_function.str();
}

void MutationReplaceExpr::ApplyCppTypeModifiers(const clang::Expr& expr,
                                                std::string& type) {
  if (expr.isLValue()) {
    type += "&";
    const clang::QualType qualified_type = expr.getType();
    if (qualified_type.isVolatileQualified()) {
      type = "volatile " + type;
    } else if (qualified_type.isConstQualified()) {
      type = "const " + type;
    }
  }
}

void MutationReplaceExpr::ApplyCTypeModifiers(const clang::Expr& expr,
                                              std::string& type) {
  if (expr.isLValue()) {
    type += "*";
    const clang::QualType qualified_type = expr.getType();
    if (qualified_type.isVolatileQualified()) {
      type = "volatile " + type;
    } else if (qualified_type.isConstQualified()) {
      type += "const " + type;
    }
  }
}

void MutationReplaceExpr::ReplaceExprWithFunctionCall(
    const std::string& new_function_name, const std::string& input_type,
    const bool semantics_preserving_mutation, int local_mutation_id,
    clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
    clang::Rewriter& rewriter) const {
  // Replacement of an expression with a function call is simulated by
  // Inserting suitable text before and after the expression.
  // This is preferable over the (otherwise more intuitive) approach of directly
  // replacing the text for the expression node, because the Clang rewriter
  // does not support nested replacements.

  // These record the text that should be inserted before and after the
  // expression.
  std::string prefix = new_function_name + "(";
  std::string suffix;

  if (!semantics_preserving_mutation && ast_context.getLangOpts().CPlusPlus &&
      expr_->HasSideEffects(ast_context)) {
    prefix.append(+"[&]() -> " + input_type + " { return " +
                  // We don't need to static cast constant expressions
                  (IsCxx11ConstantExpr(*expr_, ast_context)
                       ? ""
                       : "static_cast<" + input_type + ">("));
    suffix.append(IsCxx11ConstantExpr(*expr_, ast_context) ? "" : ")");
    suffix.append("; }");
  }

  if (!ast_context.getLangOpts().CPlusPlus) {
    if (expr_->isLValue() && input_type.ends_with('*')) {
      prefix.append("&(");
      suffix.append(")");
    } else if (const auto* binary_expr =
                   llvm::dyn_cast<clang::BinaryOperator>(expr_)) {
      // The comma operator requires special care in C, to avoid it appearing to
      // provide multiple parameters for an enclosing function call.
      if (binary_expr->isCommaOp()) {
        prefix.append("(");
        suffix.append(")");
      }
    }
  }

  suffix.append(", " + std::to_string(local_mutation_id) + ")");

  // The following code handles a tricky special case, where constant values are
  // used in an initializer list in a manner that leads to them being implicitly
  // cast. There are cases where such implicit casts are allowed for constants
  // but not for non-constants. This is catered for by inserting an explicit
  // cast.
  auto parents = ast_context.getParents<clang::Expr>(*expr_);
  // This the expression occurs in an initializer list and is subject to an
  // explicit cast then it will have two parents -- the initializer list, and
  // the implicit cast. (This is probably due to implicit casts being treated
  // as invisible nodes in the AST.)
  if (ast_context.getLangOpts().CPlusPlus && parents.size() == 2 &&
      parents[0].get<clang::InitListExpr>() != nullptr &&
      parents[1].get<clang::ImplicitCastExpr>() != nullptr) {
    // Add an explicit cast to the result type of the explicit cast.
    const auto* implicit_cast_expr = parents[1].get<clang::ImplicitCastExpr>();
    prefix = "static_cast<" +
             implicit_cast_expr->getType()
                 ->getAs<clang::BuiltinType>()
                 ->getName(ast_context.getPrintingPolicy())
                 .str() +
             ">(" + prefix;
    suffix.append(")");
  }

  const clang::SourceRange expr_source_range_in_main_file =
      GetSourceRangeInMainFile(preprocessor, *expr_);
  assert(expr_source_range_in_main_file.isValid() && "Invalid source range.");

  bool rewriter_result = rewriter.InsertTextBefore(
      expr_source_range_in_main_file.getBegin(), prefix);
  assert(!rewriter_result && "Rewrite failed.\n");
  rewriter_result = rewriter.InsertTextAfterToken(
      expr_source_range_in_main_file.getEnd(), suffix);
  assert(!rewriter_result && "Rewrite failed.\n");
  (void)rewriter_result;  // Keep release mode compilers happy.
}

protobufs::MutationGroup MutationReplaceExpr::Apply(
    clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
    bool optimise_mutations, bool semantics_preserving_mutation,
    bool only_track_mutant_coverage, int first_mutation_id_in_file,
    int& mutation_id, clang::Rewriter& rewriter,
    std::unordered_set<std::string>& dredd_declarations,
    std::unordered_set<std::string>& dredd_macros) const {
  // The protobuf object for the mutation, which will be wrapped in a
  // MutationGroup.
  protobufs::MutationReplaceExpr inner_result;

  inner_result.mutable_start()->set_line(info_for_source_range_.GetStartLine());
  inner_result.mutable_start()->set_column(
      info_for_source_range_.GetStartColumn());
  inner_result.mutable_end()->set_line(info_for_source_range_.GetEndLine());
  inner_result.mutable_end()->set_column(info_for_source_range_.GetEndColumn());
  *inner_result.mutable_snippet() = info_for_source_range_.GetSnippet();

  const std::string new_function_name =
      GetFunctionName(optimise_mutations, ast_context);
  const std::string result_type = expr_->getType()
                                      ->getAs<clang::BuiltinType>()
                                      ->getName(ast_context.getPrintingPolicy())
                                      .str();

  std::string input_type = result_type;
  // Type modifiers are added to the input type, if it is an l-value. The result
  // type is left unmodified, because l-values are only mutated in positions
  // where they are implicitly cast to r-values, so the associated mutator
  // function should return a value, not a reference.
  if (ast_context.getLangOpts().CPlusPlus) {
    ApplyCppTypeModifiers(*expr_, input_type);
  } else {
    ApplyCTypeModifiers(*expr_, input_type);
  }

  // Replace the expression with a function call.
  // Subtracting |first_mutation_id_in_file| turns the global mutation id,
  // |mutation_id|, into a file-local mutation id.
  ReplaceExprWithFunctionCall(new_function_name, input_type,
                              semantics_preserving_mutation,
                              mutation_id - first_mutation_id_in_file,
                              ast_context, preprocessor, rewriter);

  const std::string new_function = GenerateMutatorFunction(
      ast_context, dredd_macros, new_function_name, result_type, input_type,
      optimise_mutations, semantics_preserving_mutation,
      only_track_mutant_coverage, mutation_id, inner_result);
  assert(!new_function.empty() && "Unsupported expression.");

  dredd_declarations.insert(new_function);

  protobufs::MutationGroup result;
  *result.mutable_replace_expr() = inner_result;
  return result;
}

bool MutationReplaceExpr::CanMutateLValue(clang::ASTContext& ast_context,
                                          const clang::Expr& expr) {
  assert(expr.isLValue() &&
         "Method should only be invoked on an l-value expression.");
  if (expr.getType().isConstQualified() || expr.getType()->isBooleanType()) {
    return false;
  }
  // The following checks that `expr` is the child of an ImplicitCastExpr that
  // yields an r-value.
  auto parents = ast_context.getParents<clang::Expr>(expr);
  if (parents.size() != 1) {
    return false;
  }
  const auto* implicit_cast = parents[0].get<clang::ImplicitCastExpr>();
  if (implicit_cast == nullptr || implicit_cast->isLValue()) {
    return false;
  }
  return true;
}

void MutationReplaceExpr::AddMutationInstance(
    int mutation_id_base, protobufs::MutationReplaceExprAction action,
    int& mutation_id_offset, protobufs::MutationReplaceExpr& protobuf_message) {
  protobufs::MutationReplaceExprInstance instance;
  instance.set_mutation_id(mutation_id_base + mutation_id_offset);
  instance.set_action(action);
  *protobuf_message.add_instances() = instance;
  mutation_id_offset++;
}

bool MutationReplaceExpr::IsBooleanReplacementRedundantForBinaryOperator(
    bool replacement_value, const clang::ASTContext& ast_context) const {
  // From
  // https://people.cs.umass.edu/~rjust/publ/non_redundant_mutants_jstvr_2014.pdf:
  // Various cases of replacing a boolean-valued binary operator with a boolean
  // constant are redundant.
  if (const auto* binary_operator =
          llvm::dyn_cast<clang::BinaryOperator>(expr_)) {
    switch (binary_operator->getOpcode()) {
      case clang::BO_LAnd:
        // The optimisation only applies to logical operators when targeting
        // C++, since when targeting C this operator cannot be mutated with full
        // flexibility.
        return ast_context.getLangOpts().CPlusPlus && replacement_value;
      case clang::BO_LOr:
        // Again, the optimisation only applies to logical operators when
        // targeting C++.
        return ast_context.getLangOpts().CPlusPlus && !replacement_value;
      case clang::BO_GT:
      case clang::BO_LT:
      case clang::BO_EQ:
        return replacement_value;
      case clang::BO_GE:
      case clang::BO_LE:
      case clang::BO_NE:
        return !replacement_value;
      default:
        return false;
    }
  }
  return false;
}

bool MutationReplaceExpr::IsRedundantUnaryMinusInsertion(
    const clang::ASTContext& ast_context) const {
  // It never makes sense to insert '-' before 0 as this would lead to an
  // equivalent mutant. (Technically this may not be true for floating-point
  // due to two values of 0, but the mutant is likely to be equivalent.)
  if (ExprIsEquivalentToInt(*expr_, 0, ast_context) ||
      ExprIsEquivalentToFloat(*expr_, 0.0, ast_context)) {
    return true;
  }

  // If the expression is signed or floating-point, it does not make sense
  // to insert '-' before 1 or -1, as these cases are captured by
  // replacement with -1 and 1, respectively.
  if (expr_->getType()->isSignedIntegerType() &&
      (ExprIsEquivalentToInt(*expr_, 1, ast_context) ||
       ExprIsEquivalentToInt(*expr_, -1, ast_context))) {
    return true;
  }
  if (ExprIsEquivalentToFloat(*expr_, 1.0, ast_context) ||
      ExprIsEquivalentToFloat(*expr_, -1.0, ast_context)) {
    return true;
  }
  return false;
}

bool MutationReplaceExpr::IsRedundantUnaryNotInsertion(
    const clang::ASTContext& ast_context) const {
  // If the expression is signed, it does not make sense to insert '~'
  // before 0 or -1, as these cases are captured by replacement with -1 and
  // 0, respectively.
  if (expr_->getType()->isSignedIntegerType() &&
      (ExprIsEquivalentToInt(*expr_, 0, ast_context) ||
       ExprIsEquivalentToInt(*expr_, -1, ast_context))) {
    return true;
  }
  return false;
}

bool MutationReplaceExpr::IsRedundantUnaryLogicalNotInsertion(
    const clang::ASTContext& ast_context) const {
  // If the expression is a boolean constant, it does not make sense to
  // insert '!' because this is captured by replacement with the other
  // boolean constant.

  // We use this value to capture the value that the expression evaluates
  // to if it does indeed evaluate to a constant, but we do not use this
  // value because either way, operator insertion would be redundant.
  bool unused_bool_value;
  if (expr_->getType()->isBooleanType() &&
      EvaluateAsBooleanCondition(*expr_, ast_context, unused_bool_value)) {
    return true;
  }

  // If the expression is an integer constant, it does not make sense to
  // insert '!' as the result will either be 0 or 1, which is captured by
  // constant replacement.

  // Similarly, this value is not used as it does not matter which constant
  // integer the expression evaluates to.
  clang::Expr::EvalResult unused_result_value;
  if (EvaluateAsInt(*expr_, ast_context, unused_result_value)) {
    return true;
  }

  return false;
}

bool MutationReplaceExpr::IsRedundantOperatorInsertionBeforeBinaryExpr(
    const clang::ASTContext& ast_context) const {
  if (const auto* binary_operator =
          llvm::dyn_cast<clang::BinaryOperator>(expr_)) {
    switch (binary_operator->getOpcode()) {
        // From
        // https://people.cs.umass.edu/~rjust/publ/non_redundant_mutants_jstvr_2014.pdf:
        // Unary operator insertion is redundant when the expression being
        // mutated is a && b or a || b.
      case clang::BO_LAnd:
      case clang::BO_LOr:
        assert(!expr_->isLValue() &&
               "The result of one of these binary operators should be an "
               "r-value.");
        // In C, binary logical operators are not mutated with full flexibility,
        // so the optimisation mentioned above cannot be applied.
        return ast_context.getLangOpts().CPlusPlus != 0U;
        // In the following cases, unary operator insertion would be redundant
        // as it is captured by a binary operator replacement.
      case clang::BO_EQ:
      case clang::BO_NE:
      case clang::BO_GT:
      case clang::BO_GE:
      case clang::BO_LT:
      case clang::BO_LE:
        assert(!expr_->isLValue() &&
               "The result of one of these binary operators should be an "
               "r-value.");
        return true;
      default:
        return false;
    }
  }
  return false;
}

bool MutationReplaceExpr::
    IsRedundantOperatorInsertionBeforeLogicalOperatorArgument(
        clang::ASTContext& ast_context) const {
  // From
  // https://people.cs.umass.edu/~rjust/publ/non_redundant_mutants_jstvr_2014.pdf:
  // Do not replace `a && b` with `!a && b` or `a && !b`, similar for logical
  // or.

  if (!ast_context.getLangOpts().CPlusPlus) {
    // The optimisation is not performed in C mode, since there is limited
    // flexibility for mutating logical operators in C.
    return false;
  }

  auto parents = ast_context.getParents<clang::Expr>(*expr_);
  for (const auto& parent : parents) {
    if (const auto* binary_operator = parent.get<clang::BinaryOperator>()) {
      if (binary_operator->isLogicalOp() &&
          (binary_operator->getLHS() == expr_ ||
           binary_operator->getRHS() == expr_)) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace dredd
