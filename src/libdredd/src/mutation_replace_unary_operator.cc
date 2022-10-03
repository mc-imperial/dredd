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
#include "libdredd/mutation_replace_expr.h"
#include "libdredd/util.h"
#include "llvm/ADT/StringRef.h"

namespace dredd {

MutationReplaceUnaryOperator::MutationReplaceUnaryOperator(
    const clang::UnaryOperator& unary_operator,
    const clang::Preprocessor& preprocessor,
    const clang::ASTContext& ast_context)
    : unary_operator_(unary_operator),
      info_for_overall_expr_(
          GetSourceRangeInMainFile(preprocessor, unary_operator), ast_context),
      info_for_sub_expr_(
          GetSourceRangeInMainFile(preprocessor, *unary_operator.getSubExpr()),
          ast_context) {}

bool MutationReplaceUnaryOperator::IsPrefix(
    clang::UnaryOperatorKind operator_kind) {
  return operator_kind != clang::UO_PostInc &&
         operator_kind != clang::UO_PostDec;
}

std::string MutationReplaceUnaryOperator::GetExpr(
    clang::ASTContext& ast_context) const {
  std::string arg_evaluated = "arg";

  if (ast_context.getLangOpts().CPlusPlus) {
    arg_evaluated += "()";
  } else {
    if (unary_operator_.isIncrementDecrementOp()) {
      arg_evaluated = "(*" + arg_evaluated + ")";
    }
  }

  std::string result;
  if (IsPrefix(unary_operator_.getOpcode())) {
    result =
        clang::UnaryOperator::getOpcodeStr(unary_operator_.getOpcode()).str() +
        arg_evaluated;
  } else {
    result =
        arg_evaluated +
        clang::UnaryOperator::getOpcodeStr(unary_operator_.getOpcode()).str();
  }
  return result;
}

bool MutationReplaceUnaryOperator::IsValidReplacementOperator(
    clang::UnaryOperatorKind operator_kind) const {
  if (!unary_operator_.getSubExpr()->isLValue() &&
      (operator_kind == clang::UO_PreInc || operator_kind == clang::UO_PreDec ||
       operator_kind == clang::UO_PostInc ||
       operator_kind == clang::UO_PostDec)) {
    // The increment and decrement operators require an l-value.
    return false;
  }

  if ((unary_operator_.getOpcode() == clang::UO_PostDec ||
       unary_operator_.getOpcode() == clang::UO_PostInc) &&
      (operator_kind == clang::UO_PreDec ||
       operator_kind == clang::UO_PreInc)) {
    // Do not replace a post-increment/decrement with a pre-increment/decrement;
    // it's unlikely to be interesting.
    return false;
  }

  if ((unary_operator_.getOpcode() == clang::UO_PreDec ||
       unary_operator_.getOpcode() == clang::UO_PreInc) &&
      (operator_kind == clang::UO_PostDec ||
       operator_kind == clang::UO_PostInc)) {
    // Do not replace a pre-increment/decrement with a pre-increment/decrement;
    // it's unlikely to be interesting.
    return false;
  }

  if (unary_operator_.isLValue() && !(operator_kind == clang::UO_PreInc ||
                                      operator_kind == clang::UO_PreDec)) {
    // In C++, only pre-increment/decrement operations yield an l-value.
    return false;
  }

  if (operator_kind == clang::UO_Not && unary_operator_.getSubExpr()
                                            ->getType()
                                            ->getAs<clang::BuiltinType>()
                                            ->isFloatingPoint()) {
    return false;
  }

  return true;
}

std::string MutationReplaceUnaryOperator::GetFunctionName(
    bool optimise_mutations, clang::ASTContext& ast_context) const {
  std::string result = "__dredd_replace_unary_operator_";

  // A string corresponding to the unary operator forms part of the name of the
  // mutation function, to differentiate mutation functions for different
  // operators
  switch (unary_operator_.getOpcode()) {
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

  // In the case that we can optimise out some unary expressions, it is
  // important to change the name of the mutator function to avoid clashes
  // with other versions that apply to the same operator and types but cannot
  // be optimised.
  if (optimise_mutations) {
    if (MutationReplaceExpr::ExprIsEquivalentToInt(
            *unary_operator_.getSubExpr(), 0, ast_context) ||
        MutationReplaceExpr::ExprIsEquivalentToFloat(
            *unary_operator_.getSubExpr(), 0.0, ast_context)) {
      result += "_zero";
    }

    if (MutationReplaceExpr::ExprIsEquivalentToInt(
            *unary_operator_.getSubExpr(), 1, ast_context) ||
        MutationReplaceExpr::ExprIsEquivalentToFloat(
            *unary_operator_.getSubExpr(), 1.0, ast_context)) {
      result += "_one";
    }

    if (MutationReplaceExpr::ExprIsEquivalentToInt(
            *unary_operator_.getSubExpr(), -1, ast_context) ||
        MutationReplaceExpr::ExprIsEquivalentToFloat(
            *unary_operator_.getSubExpr(), -1.0, ast_context)) {
      result += "_minus_one";
    }
  }

  return result;
}

std::string MutationReplaceUnaryOperator::GenerateMutatorFunction(
    clang::ASTContext& ast_context, const std::string& function_name,
    const std::string& result_type, const std::string& input_type,
    bool optimise_mutations, int& mutation_id,
    protobufs::MutationReplaceUnaryOperator& protobuf_message) const {
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

  int mutation_id_offset = 0;
  std::vector<clang::UnaryOperatorKind> operators = {
      clang::UnaryOperatorKind::UO_PreInc, clang::UnaryOperatorKind::UO_PostInc,
      clang::UnaryOperatorKind::UO_PreDec, clang::UnaryOperatorKind::UO_PostDec,
      clang::UnaryOperatorKind::UO_Not,    clang::UnaryOperatorKind::UO_Minus,
      clang::UnaryOperatorKind::UO_LNot};

  GenerateUnaryOperatorReplacement(
      arg_evaluated, ast_context, optimise_mutations, mutation_id, operators,
      new_function, mutation_id_offset, protobuf_message);

  new_function << "  return " << GetExpr(ast_context) << ";\n";
  new_function << "}\n\n";

  // The function captures |mutation_id_offset| different mutations, so bump up
  // the mutation id accordingly.
  mutation_id += mutation_id_offset;

  return new_function.str();
}

bool MutationReplaceUnaryOperator::IsRedundantReplacementOperator(
    clang::UnaryOperatorKind operator_kind,
    clang::ASTContext& ast_context) const {
  // When the operand is 0: - is equivalent to replacement with 0 and ! is
  // equivalent to replacement with 1. When the operand is 1: - is equivalent to
  // replacement with -1 and ! is equivalent to replacement with 0.
  if (MutationReplaceExpr::ExprIsEquivalentToInt(*unary_operator_.getSubExpr(),
                                                 0, ast_context) ||
      MutationReplaceExpr::ExprIsEquivalentToFloat(
          *unary_operator_.getSubExpr(), 0.0, ast_context) ||
      MutationReplaceExpr::ExprIsEquivalentToInt(*unary_operator_.getSubExpr(),
                                                 1, ast_context) ||
      MutationReplaceExpr::ExprIsEquivalentToFloat(
          *unary_operator_.getSubExpr(), 1.0, ast_context)) {
    if (operator_kind == clang::UO_Minus || operator_kind == clang::UO_LNot) {
      return true;
    }
  }

  // When the operand is -1: - is equivalent to replacement with 1.
  if ((MutationReplaceExpr::ExprIsEquivalentToInt(*unary_operator_.getSubExpr(),
                                                  -1, ast_context) ||
       MutationReplaceExpr::ExprIsEquivalentToFloat(
           *unary_operator_.getSubExpr(), -1.0, ast_context)) &&
      operator_kind == clang::UO_Minus) {
    return true;
  }

  return false;
}

void MutationReplaceUnaryOperator::GenerateUnaryOperatorReplacement(
    const std::string& arg_evaluated, clang::ASTContext& ast_context,
    bool optimise_mutations, int mutation_id_base,
    const std::vector<clang::UnaryOperatorKind>& operators,
    std::stringstream& new_function, int& mutation_id_offset,
    protobufs::MutationReplaceUnaryOperator& protobuf_message) const {
  for (const auto operator_kind : operators) {
    if (operator_kind == unary_operator_.getOpcode() ||
        !IsValidReplacementOperator(operator_kind) ||
        (optimise_mutations &&
         IsRedundantReplacementOperator(operator_kind, ast_context))) {
      continue;
    }
    new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                 << mutation_id_offset << ")) return ";
    if (IsPrefix(operator_kind)) {
      new_function << clang::UnaryOperator::getOpcodeStr(operator_kind).str()
                   << arg_evaluated + ";\n";
    } else {
      new_function << arg_evaluated
                   << clang::UnaryOperator::getOpcodeStr(operator_kind).str()
                   << ";\n";
    }
    AddMutationInstance(mutation_id_base, OperatorKindToAction(operator_kind),
                        mutation_id_offset, protobuf_message);
  }

  // In these cases, replacement with the argument is equivalent to replacement
  // with the respective constant.
  if (!optimise_mutations ||
      MutationReplaceExpr::ExprIsEquivalentToInt(*unary_operator_.getSubExpr(),
                                                 0, ast_context) ||
      MutationReplaceExpr::ExprIsEquivalentToFloat(
          *unary_operator_.getSubExpr(), 0.0, ast_context) ||
      MutationReplaceExpr::ExprIsEquivalentToInt(*unary_operator_.getSubExpr(),
                                                 1, ast_context) ||
      MutationReplaceExpr::ExprIsEquivalentToFloat(
          *unary_operator_.getSubExpr(), 1.0, ast_context) ||
      MutationReplaceExpr::ExprIsEquivalentToInt(*unary_operator_.getSubExpr(),
                                                 -1, ast_context) ||
      MutationReplaceExpr::ExprIsEquivalentToFloat(
          *unary_operator_.getSubExpr(), -1.0, ast_context)) {
    new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                 << mutation_id_offset << ")) return " + arg_evaluated + ";\n";
    AddMutationInstance(
        mutation_id_base,
        protobufs::MutationReplaceUnaryOperatorAction::ReplaceWithOperand,
        mutation_id_offset, protobuf_message);
  }
}

protobufs::MutationGroup MutationReplaceUnaryOperator::Apply(
    clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
    bool optimise_mutations, int first_mutation_id_in_file, int& mutation_id,
    clang::Rewriter& rewriter,
    std::unordered_set<std::string>& dredd_declarations) const {
  // The protobuf object for the mutation, which will be wrapped in a
  // MutationGroup.
  protobufs::MutationReplaceUnaryOperator inner_result;

  inner_result.mutable_expr_start()->set_line(
      info_for_overall_expr_.GetStartLine());
  inner_result.mutable_expr_start()->set_column(
      info_for_overall_expr_.GetStartColumn());
  inner_result.mutable_expr_end()->set_line(
      info_for_overall_expr_.GetEndLine());
  inner_result.mutable_expr_end()->set_column(
      info_for_overall_expr_.GetEndColumn());
  *inner_result.mutable_expr_snippet() = info_for_overall_expr_.GetSnippet();

  inner_result.mutable_operand_start()->set_line(
      info_for_sub_expr_.GetStartLine());
  inner_result.mutable_operand_start()->set_column(
      info_for_sub_expr_.GetStartColumn());
  inner_result.mutable_operand_end()->set_line(info_for_sub_expr_.GetEndLine());
  inner_result.mutable_operand_end()->set_column(
      info_for_sub_expr_.GetEndColumn());
  *inner_result.mutable_operand_snippet() = info_for_sub_expr_.GetSnippet();

  std::string new_function_name =
      GetFunctionName(optimise_mutations, ast_context);
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
    MutationReplaceExpr::ApplyCppTypeModifiers(unary_operator_.getSubExpr(),
                                               input_type);
    MutationReplaceExpr::ApplyCppTypeModifiers(&unary_operator_, result_type);
  } else {
    MutationReplaceExpr::ApplyCTypeModifiers(unary_operator_.getSubExpr(),
                                             input_type);
  }

  clang::SourceRange unary_operator_source_range_in_main_file =
      GetSourceRangeInMainFile(preprocessor, unary_operator_);
  assert(unary_operator_source_range_in_main_file.isValid() &&
         "Invalid source range.");

  // Replace the unary operator expression with a call to the wrapper
  // function.
  //
  // Subtracting |first_mutation_id_in_file| turns the global mutation id,
  // |mutation_id|, into a file-local mutation id.
  const int local_mutation_id = mutation_id - first_mutation_id_in_file;

  // Replacement of a unary operator with a function call is simulated by:
  // - Removing the text associated with the unary operator symbol.
  // - Inserting suitable text before and after the argument to the unary
  //   operator.
  // This is preferable over the (otherwise more intuitive) approach of directly
  // replacing the text for the unary operator node, because the Clang rewriter
  // does not support nested replacements.

  // Remove the operator symbol.
  rewriter.ReplaceText(
      unary_operator_.getOperatorLoc(),
      static_cast<unsigned int>(
          clang::UnaryOperator::getOpcodeStr(unary_operator_.getOpcode())
              .size()),
      "");

  // These record the text that should be inserted before and after the operand.
  std::string prefix;
  std::string suffix;
  if (ast_context.getLangOpts().CPlusPlus) {
    prefix = new_function_name + "([&]() -> " + input_type + " { return " +
             // We don't need to static cast constant expressions
             (unary_operator_.getSubExpr()->isCXX11ConstantExpr(ast_context)
                  ? ""
                  : "static_cast<" + input_type + ">(");
    suffix =
        (unary_operator_.getSubExpr()->isCXX11ConstantExpr(ast_context) ? ""
                                                                        : ")");
    suffix.append("; }, " + std::to_string(local_mutation_id) + ")");
  } else {
    prefix = new_function_name + "(";
    if (unary_operator_.isIncrementDecrementOp()) {
      prefix.append("&(");
      suffix.append(")");
    }
    suffix.append(", " + std::to_string(local_mutation_id) + ")");
  }
  // The prefix and suffix are ready, so make the relevant insertions.
  bool rewriter_result = rewriter.InsertTextBefore(
      unary_operator_source_range_in_main_file.getBegin(), prefix);
  assert(!rewriter_result && "Rewrite failed.\n");
  rewriter_result = rewriter.InsertTextAfterToken(
      unary_operator_source_range_in_main_file.getEnd(), suffix);
  assert(!rewriter_result && "Rewrite failed.\n");
  (void)rewriter_result;  // Keep release-mode compilers happy.

  std::string new_function = GenerateMutatorFunction(
      ast_context, new_function_name, result_type, input_type,
      optimise_mutations, mutation_id, inner_result);
  assert(!new_function.empty() && "Unsupported opcode.");

  dredd_declarations.insert(new_function);

  protobufs::MutationGroup result;
  *result.mutable_replace_unary_operator() = inner_result;
  return result;
}

void MutationReplaceUnaryOperator::AddMutationInstance(
    int mutation_id_base, protobufs::MutationReplaceUnaryOperatorAction action,
    int& mutation_id_offset,
    protobufs::MutationReplaceUnaryOperator& protobuf_message) {
  protobufs::MutationReplaceUnaryOperatorInstance instance;
  instance.set_mutation_id(mutation_id_base + mutation_id_offset);
  instance.set_action(action);
  *protobuf_message.add_instances() = instance;
  mutation_id_offset++;
}

protobufs::MutationReplaceUnaryOperatorAction
MutationReplaceUnaryOperator::OperatorKindToAction(
    clang::UnaryOperatorKind operator_kind) {
  switch (operator_kind) {
    case clang::UnaryOperatorKind::UO_Minus:
      return protobufs::MutationReplaceUnaryOperatorAction::ReplaceWithMinus;
    case clang::UnaryOperatorKind::UO_Not:
      return protobufs::MutationReplaceUnaryOperatorAction::ReplaceWithNot;
    case clang::UnaryOperatorKind::UO_PreDec:
      return protobufs::MutationReplaceUnaryOperatorAction::ReplaceWithPreDec;
    case clang::UnaryOperatorKind::UO_PostDec:
      return protobufs::MutationReplaceUnaryOperatorAction::ReplaceWithPostDec;
    case clang::UnaryOperatorKind::UO_PreInc:
      return protobufs::MutationReplaceUnaryOperatorAction::ReplaceWithPreInc;
    case clang::UnaryOperatorKind::UO_PostInc:
      return protobufs::MutationReplaceUnaryOperatorAction::ReplaceWithPostInc;
    case clang::UnaryOperatorKind::UO_LNot:
      return protobufs::MutationReplaceUnaryOperatorAction::ReplaceWithLNot;
    default:
      return protobufs::MutationReplaceUnaryOperatorAction_MAX;
  }
}

}  // namespace dredd
