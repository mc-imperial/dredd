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
#include "libdredd/mutation_replace_expr.h"
#include "libdredd/util.h"
#include "llvm/ADT/StringRef.h"

namespace dredd {

MutationReplaceBinaryOperator::MutationReplaceBinaryOperator(
    const clang::BinaryOperator& binary_operator,
    const clang::Preprocessor& preprocessor,
    const clang::ASTContext& ast_context)
    : binary_operator_(&binary_operator),
      info_for_overall_expr_(
          GetSourceRangeInMainFile(preprocessor, binary_operator), ast_context),
      info_for_lhs_(
          GetSourceRangeInMainFile(preprocessor, *binary_operator.getLHS()),
          ast_context),
      info_for_rhs_(
          GetSourceRangeInMainFile(preprocessor, *binary_operator.getRHS()),
          ast_context) {}

bool MutationReplaceBinaryOperator::IsRedundantReplacementOperator(
    clang::BinaryOperatorKind operator_kind,
    const clang::ASTContext& ast_context) const {
  if (IsRedundantReplacementForBooleanValuedOperator(operator_kind)) {
    return true;
  }
  if (IsRedundantReplacementForArithmeticOperator(operator_kind, ast_context)) {
    return true;
  }
  return false;
}

// Certain operators such as % are not compatible with floating point numbers,
// this function checks which operators can be applied for each type.
bool MutationReplaceBinaryOperator::IsValidReplacementOperator(
    clang::BinaryOperatorKind operator_kind) const {
  const auto* lhs_type =
      binary_operator_->getLHS()->getType()->getAs<clang::BuiltinType>();
  assert((lhs_type->isFloatingPoint() || lhs_type->isInteger()) &&
         "Expected lhs to be either integer or floating-point.");
  const auto* rhs_type =
      binary_operator_->getRHS()->getType()->getAs<clang::BuiltinType>();
  assert((rhs_type->isFloatingPoint() || rhs_type->isInteger()) &&
         "Expected rhs to be either integer or floating-point.");
  return !((lhs_type->isFloatingPoint() || rhs_type->isFloatingPoint()) &&
           (operator_kind == clang::BO_Rem ||
            operator_kind == clang::BO_AndAssign ||
            operator_kind == clang::BO_OrAssign ||
            operator_kind == clang::BO_RemAssign ||
            operator_kind == clang::BO_ShlAssign ||
            operator_kind == clang::BO_ShrAssign ||
            operator_kind == clang::BO_XorAssign));
}

std::string MutationReplaceBinaryOperator::GetFunctionName(
    bool optimise_mutations, clang::ASTContext& ast_context) const {
  std::string result = "__dredd_replace_binary_operator_";

  // A string corresponding to the binary operator forms part of the name of the
  // mutation function, to differentiate mutation functions for different
  // operators
  switch (binary_operator_->getOpcode()) {
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

  if (binary_operator_->isAssignmentOp()) {
    const clang::QualType qualified_lhs_type =
        binary_operator_->getLHS()->getType();
    if (qualified_lhs_type.isVolatileQualified()) {
      lhs_qualifier = "volatile ";
    }
  }
  // To avoid problems of ambiguous function calls, the argument types (ignoring
  // whether they are references or not) are baked into the mutation function
  // name. Some type names have space in them (e.g. 'unsigned int'); such spaces
  // are replaced with underscores. We add arg specifiers to avoid the ambiguous
  // case when the arguments are `long long` and `long`.
  result += "_arg1_" +
            SpaceToUnderscore(lhs_qualifier +
                              binary_operator_->getLHS()
                                  ->getType()
                                  ->getAs<clang::BuiltinType>()
                                  ->getName(ast_context.getPrintingPolicy())
                                  .str());
  result += "_arg2_" +
            SpaceToUnderscore(binary_operator_->getRHS()
                                  ->getType()
                                  ->getAs<clang::BuiltinType>()
                                  ->getName(ast_context.getPrintingPolicy())
                                  .str());

  // In the case that we can optimise out some binary expressions, it is
  // important to change the name of the mutator function to avoid clashes
  // with other versions that apply to the same operator and types but cannot
  // be optimised.
  if (optimise_mutations && !binary_operator_->isAssignmentOp()) {
    if (MutationReplaceExpr::ExprIsEquivalentToInt(*binary_operator_->getRHS(),
                                                   0, ast_context) ||
        MutationReplaceExpr::ExprIsEquivalentToFloat(
            *binary_operator_->getRHS(), 0.0, ast_context)) {
      result += "_rhs_zero";
    } else if (MutationReplaceExpr::ExprIsEquivalentToInt(
                   *binary_operator_->getRHS(), 1, ast_context) ||
               MutationReplaceExpr::ExprIsEquivalentToFloat(
                   *binary_operator_->getRHS(), 1.0, ast_context)) {
      result += "_rhs_one";
    } else if (MutationReplaceExpr::ExprIsEquivalentToInt(
                   *binary_operator_->getRHS(), -1, ast_context) ||
               MutationReplaceExpr::ExprIsEquivalentToFloat(
                   *binary_operator_->getRHS(), -1.0, ast_context)) {
      result += "_rhs_minus_one";
    }

    if (MutationReplaceExpr::ExprIsEquivalentToInt(*binary_operator_->getLHS(),
                                                   0, ast_context) ||
        MutationReplaceExpr::ExprIsEquivalentToFloat(
            *binary_operator_->getLHS(), 0.0, ast_context)) {
      result += "_lhs_zero";
    } else if (MutationReplaceExpr::ExprIsEquivalentToInt(
                   *binary_operator_->getLHS(), 1, ast_context) ||
               MutationReplaceExpr::ExprIsEquivalentToFloat(
                   *binary_operator_->getLHS(), 1.0, ast_context)) {
      result += "_lhs_one";
    } else if (MutationReplaceExpr::ExprIsEquivalentToInt(
                   *binary_operator_->getLHS(), -1, ast_context) ||
               MutationReplaceExpr::ExprIsEquivalentToFloat(
                   *binary_operator_->getLHS(), -1.0, ast_context)) {
      result += "_lhs_minus_one";
    }
  }

  return result;
}

void MutationReplaceBinaryOperator::GenerateArgumentReplacement(
    const std::string& arg1_evaluated, const std::string& arg2_evaluated,
    const clang::ASTContext& ast_context, bool optimise_mutations,
    bool only_track_mutant_coverage, int mutation_id_base,
    std::stringstream& new_function, int& mutation_id_offset,
    protobufs::MutationReplaceBinaryOperator& protobuf_message) const {
  if (optimise_mutations) {
    switch (binary_operator_->getOpcode()) {
      case clang::BO_GT:
      case clang::BO_GE:
      case clang::BO_LT:
      case clang::BO_LE:
      case clang::BO_EQ:
      case clang::BO_NE:
        // Even though it is type-correct in C/C++ to replace the result of a
        // relational operator with one of its arguments, this will typically be
        // uninteresting and almost certainly subsumed by other mutations.
        return;
      default:
        break;
    }
  }

  if (binary_operator_->isAssignmentOp()) {
    // It would be possible to replace an assignment operator, such as `x = y`,
    // with its LHS. However, since the most common case is for such expressions
    // to appear as top-level statements, with the LHS being a side effect-free
    // expression, this replacement will almost always be equivalent to removing
    // the enclosing statement.
    return;
  }
  // LHS
  // These cases are equivalent to constant replacement with the respective
  // constants
  if (!optimise_mutations ||
      !(MutationReplaceExpr::ExprIsEquivalentToInt(*binary_operator_->getLHS(),
                                                   0, ast_context) ||
        MutationReplaceExpr::ExprIsEquivalentToFloat(
            *binary_operator_->getLHS(), 0.0, ast_context) ||
        MutationReplaceExpr::ExprIsEquivalentToInt(*binary_operator_->getLHS(),
                                                   1, ast_context) ||
        MutationReplaceExpr::ExprIsEquivalentToFloat(
            *binary_operator_->getLHS(), 1.0, ast_context) ||
        MutationReplaceExpr::ExprIsEquivalentToInt(*binary_operator_->getLHS(),
                                                   -1, ast_context) ||
        MutationReplaceExpr::ExprIsEquivalentToFloat(
            *binary_operator_->getLHS(), -1.0, ast_context))) {
    if (!only_track_mutant_coverage) {
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutation_id_offset << ")) return " << arg1_evaluated
                   << ";\n";
    }
    AddMutationInstance(
        mutation_id_base,
        protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithLHS,
        mutation_id_offset, protobuf_message);
  }

  // RHS
  // These cases are equivalent to constant replacement with the respective
  // constants
  if (!optimise_mutations ||
      !(MutationReplaceExpr::ExprIsEquivalentToInt(*binary_operator_->getRHS(),
                                                   0, ast_context) ||
        MutationReplaceExpr::ExprIsEquivalentToFloat(
            *binary_operator_->getRHS(), 0.0, ast_context) ||
        MutationReplaceExpr::ExprIsEquivalentToInt(*binary_operator_->getRHS(),
                                                   1, ast_context) ||
        MutationReplaceExpr::ExprIsEquivalentToFloat(
            *binary_operator_->getRHS(), 1.0, ast_context) ||
        MutationReplaceExpr::ExprIsEquivalentToInt(*binary_operator_->getRHS(),
                                                   -1, ast_context) ||
        MutationReplaceExpr::ExprIsEquivalentToFloat(
            *binary_operator_->getRHS(), -1.0, ast_context))) {
    if (!only_track_mutant_coverage) {
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutation_id_offset << ")) return " << arg2_evaluated
                   << ";\n";
    }
    AddMutationInstance(
        mutation_id_base,
        protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithRHS,
        mutation_id_offset, protobuf_message);
  }
}

void MutationReplaceBinaryOperator::GenerateBinaryOperatorReplacement(
    const std::string& arg1_evaluated, const std::string& arg2_evaluated,
    const clang::ASTContext& ast_context, bool optimise_mutations,
    bool only_track_mutant_coverage, int mutation_id_base,
    std::stringstream& new_function, int& mutation_id_offset,
    protobufs::MutationReplaceBinaryOperator& protobuf_message) const {
  for (auto operator_kind :
       GetReplacementOperators(optimise_mutations, ast_context)) {
    if (!only_track_mutant_coverage) {
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutation_id_offset << ")) return " << arg1_evaluated
                   << " "
                   << clang::BinaryOperator::getOpcodeStr(operator_kind).str()
                   << " " << arg2_evaluated << ";\n";
    }
    AddMutationInstance(mutation_id_base, OperatorKindToAction(operator_kind),
                        mutation_id_offset, protobuf_message);
  }
}

std::vector<clang::BinaryOperatorKind>
MutationReplaceBinaryOperator::GetReplacementOperators(
    bool optimise_mutations, const clang::ASTContext& ast_context) const {
  const std::vector<clang::BinaryOperatorKind> kArithmeticOperators = {
      clang::BinaryOperatorKind::BO_Add, clang::BinaryOperatorKind::BO_Div,
      clang::BinaryOperatorKind::BO_Mul, clang::BinaryOperatorKind::BO_Rem,
      clang::BinaryOperatorKind::BO_Sub};

  const std::vector<clang::BinaryOperatorKind> kAssignmentOperators = {
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

  const std::vector<clang::BinaryOperatorKind> kBitwiseOperators = {
      clang::BinaryOperatorKind::BO_And, clang::BinaryOperatorKind::BO_Or,
      clang::BinaryOperatorKind::BO_Xor};

  const std::vector<clang::BinaryOperatorKind> kLogicalOperators = {
      clang::BinaryOperatorKind::BO_LAnd, clang::BinaryOperatorKind::BO_LOr};

  const std::vector<clang::BinaryOperatorKind> kRelationalOperators = {
      clang::BinaryOperatorKind::BO_EQ, clang::BinaryOperatorKind::BO_NE,
      clang::BinaryOperatorKind::BO_GE, clang::BinaryOperatorKind::BO_GT,
      clang::BinaryOperatorKind::BO_LE, clang::BinaryOperatorKind::BO_LT};

  const std::vector<clang::BinaryOperatorKind> kShiftOperators = {
      clang::BinaryOperatorKind::BO_Shl, clang::BinaryOperatorKind::BO_Shr};

  std::vector<clang::BinaryOperatorKind> candidate_operator_kinds;
  for (const auto& operator_kinds :
       {kArithmeticOperators, kAssignmentOperators, kBitwiseOperators,
        kLogicalOperators, kRelationalOperators, kShiftOperators}) {
    if (std::find(operator_kinds.begin(), operator_kinds.end(),
                  binary_operator_->getOpcode()) != operator_kinds.end()) {
      candidate_operator_kinds = operator_kinds;
      // It is desirable to consider replacing && and || with == and !=.
      if (binary_operator_->getOpcode() == clang::BO_LAnd ||
          binary_operator_->getOpcode() == clang::BO_LOr) {
        candidate_operator_kinds.push_back(clang::BO_EQ);
        candidate_operator_kinds.push_back(clang::BO_NE);
      }
      // When they are applied to booleans, it is desirable to consider
      // replacing == and != with && and ||.
      if (binary_operator_->getLHS()->getType()->isBooleanType() &&
          (binary_operator_->getOpcode() == clang::BO_EQ ||
           binary_operator_->getOpcode() == clang::BO_NE)) {
        candidate_operator_kinds.push_back(clang::BO_LAnd);
        candidate_operator_kinds.push_back(clang::BO_LOr);
      }
      break;
    }
  }

  std::vector<clang::BinaryOperatorKind> result;
  for (auto operator_kind : candidate_operator_kinds) {
    if (operator_kind == binary_operator_->getOpcode() ||
        !IsValidReplacementOperator(operator_kind) ||
        (optimise_mutations &&
         IsRedundantReplacementOperator(operator_kind, ast_context))) {
      continue;
    }
    result.push_back(operator_kind);
  }
  return result;
}

std::string MutationReplaceBinaryOperator::GenerateMutatorFunction(
    clang::ASTContext& ast_context, const std::string& function_name,
    const std::string& result_type, const std::string& lhs_type,
    const std::string& rhs_type, bool optimise_mutations,
    bool only_track_mutant_coverage, int& mutation_id,
    protobufs::MutationReplaceBinaryOperator& protobuf_message) const {
  std::stringstream new_function;
  new_function << "static " << result_type << " " << function_name << "(";

  if (ast_context.getLangOpts().CPlusPlus &&
      binary_operator_->getLHS()->HasSideEffects(ast_context)) {
    new_function << "std::function<" << lhs_type << "()>";
  } else {
    new_function << lhs_type;
  }
  new_function << " arg1, ";

  if (ast_context.getLangOpts().CPlusPlus &&
      (binary_operator_->isLogicalOp() ||
       binary_operator_->getRHS()->HasSideEffects(ast_context))) {
    new_function << "std::function<" << rhs_type << "()>";
  } else {
    new_function << rhs_type;
  }

  new_function << " arg2, int local_mutation_id) {\n";

  int mutation_id_offset = 0;

  std::string arg1_evaluated("arg1");
  if (ast_context.getLangOpts().CPlusPlus &&
      binary_operator_->getLHS()->HasSideEffects(ast_context)) {
    arg1_evaluated += "()";
  }
  if (!ast_context.getLangOpts().CPlusPlus &&
      binary_operator_->isAssignmentOp()) {
    arg1_evaluated = "(*" + arg1_evaluated + ")";
  }

  std::string arg2_evaluated("arg2");
  if (ast_context.getLangOpts().CPlusPlus &&
      (binary_operator_->isLogicalOp() ||
       binary_operator_->getRHS()->HasSideEffects(ast_context))) {
    arg2_evaluated += "()";
  }

  if (!only_track_mutant_coverage) {
    // Quickly apply the original operator if no mutant is enabled (which will
    // be the common case).
    new_function << "  if (!__dredd_some_mutation_enabled) return "
                 << arg1_evaluated << " "
                 << clang::BinaryOperator::getOpcodeStr(
                        binary_operator_->getOpcode())
                        .str()
                 << " " << arg2_evaluated << ";\n";
  }

  GenerateBinaryOperatorReplacement(
      arg1_evaluated, arg2_evaluated, ast_context, optimise_mutations,
      only_track_mutant_coverage, mutation_id, new_function, mutation_id_offset,
      protobuf_message);
  GenerateArgumentReplacement(arg1_evaluated, arg2_evaluated, ast_context,
                              optimise_mutations, only_track_mutant_coverage,
                              mutation_id, new_function, mutation_id_offset,
                              protobuf_message);

  if (only_track_mutant_coverage) {
    new_function << "  __dredd_record_covered_mutants(local_mutation_id, " +
                        std::to_string(mutation_id_offset) + ");\n";
  }
  new_function << "  return " << arg1_evaluated << " "
               << clang::BinaryOperator::getOpcodeStr(
                      binary_operator_->getOpcode())
                      .str()
               << " " << arg2_evaluated << ";\n";

  new_function << "}\n\n";

  // The function captures |mutation_id_offset| different mutations, so bump up
  // the mutation id accordingly.
  mutation_id += mutation_id_offset;

  return new_function.str();
}

protobufs::MutationGroup MutationReplaceBinaryOperator::Apply(
    clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
    bool optimise_mutations, bool only_track_mutant_coverage,
    int first_mutation_id_in_file, int& mutation_id, clang::Rewriter& rewriter,
    std::unordered_set<std::string>& dredd_declarations) const {
  // The protobuf object for the mutation, which will be wrapped in a
  // MutationGroup.
  protobufs::MutationReplaceBinaryOperator inner_result;

  inner_result.set_operator_(
      ClangOperatorKindToProtobufOperatorKind(binary_operator_->getOpcode()));

  inner_result.mutable_expr_start()->set_line(
      info_for_overall_expr_.GetStartLine());
  inner_result.mutable_expr_start()->set_column(
      info_for_overall_expr_.GetStartColumn());
  inner_result.mutable_expr_end()->set_line(
      info_for_overall_expr_.GetEndLine());
  inner_result.mutable_expr_end()->set_column(
      info_for_overall_expr_.GetEndColumn());
  *inner_result.mutable_expr_snippet() = info_for_overall_expr_.GetSnippet();

  inner_result.mutable_lhs_start()->set_line(info_for_lhs_.GetStartLine());
  inner_result.mutable_lhs_start()->set_column(info_for_lhs_.GetStartColumn());
  inner_result.mutable_lhs_end()->set_line(info_for_lhs_.GetEndLine());
  inner_result.mutable_lhs_end()->set_column(info_for_lhs_.GetEndColumn());
  *inner_result.mutable_lhs_snippet() = info_for_lhs_.GetSnippet();

  inner_result.mutable_rhs_start()->set_line(info_for_rhs_.GetStartLine());
  inner_result.mutable_rhs_start()->set_column(info_for_rhs_.GetStartColumn());
  inner_result.mutable_rhs_end()->set_line(info_for_rhs_.GetEndLine());
  inner_result.mutable_rhs_end()->set_column(info_for_rhs_.GetEndColumn());
  *inner_result.mutable_rhs_snippet() = info_for_rhs_.GetSnippet();

  const std::string new_function_name =
      GetFunctionName(optimise_mutations, ast_context);
  std::string result_type = binary_operator_->getType()
                                ->getAs<clang::BuiltinType>()
                                ->getName(ast_context.getPrintingPolicy())
                                .str();
  std::string lhs_type = binary_operator_->getLHS()
                             ->getType()
                             ->getAs<clang::BuiltinType>()
                             ->getName(ast_context.getPrintingPolicy())
                             .str();
  const std::string rhs_type = binary_operator_->getRHS()
                                   ->getType()
                                   ->getAs<clang::BuiltinType>()
                                   ->getName(ast_context.getPrintingPolicy())
                                   .str();

  if (!ast_context.getLangOpts().CPlusPlus && binary_operator_->isLogicalOp()) {
    // Logical operators in C require special treatment (see the header file for
    // details). Rather than scattering this special treatment throughout the
    // logic for handling other operators, it is simpler to handle this case
    // separately.
    HandleCLogicalOperator(preprocessor, new_function_name, result_type,
                           lhs_type, rhs_type, only_track_mutant_coverage,
                           first_mutation_id_in_file, mutation_id, rewriter,
                           dredd_declarations);

    protobufs::MutationGroup result;
    *result.mutable_replace_binary_operator() = inner_result;
    return result;
  }

  if (binary_operator_->isAssignmentOp()) {
    if (ast_context.getLangOpts().CPlusPlus) {
      result_type += "&";
      lhs_type += "&";
    } else {
      lhs_type += "*";
    }
    const clang::QualType qualified_lhs_type =
        binary_operator_->getLHS()->getType();
    if (qualified_lhs_type.isVolatileQualified()) {
      lhs_type = "volatile " + lhs_type;
      if (ast_context.getLangOpts().CPlusPlus) {
        assert(binary_operator_->getType().isVolatileQualified() &&
               "Expected expression to be volatile-qualified since LHS is.");
        result_type = "volatile " + result_type;
      }
    }
  }

  ReplaceOperator(lhs_type, rhs_type, new_function_name, ast_context,
                  preprocessor, first_mutation_id_in_file, mutation_id,
                  rewriter);

  const std::string new_function = GenerateMutatorFunction(
      ast_context, new_function_name, result_type, lhs_type, rhs_type,
      optimise_mutations, only_track_mutant_coverage, mutation_id,
      inner_result);
  assert(!new_function.empty() && "Unsupported opcode.");

  // Add the mutation function to the set of Dredd declarations - there may
  // already be a matching function, in which case duplication will be avoided.
  dredd_declarations.insert(new_function);

  protobufs::MutationGroup result;
  *result.mutable_replace_binary_operator() = inner_result;
  return result;
}

void MutationReplaceBinaryOperator::ReplaceOperator(
    const std::string& lhs_type, const std::string& rhs_type,
    const std::string& new_function_name, clang::ASTContext& ast_context,
    const clang::Preprocessor& preprocessor, int first_mutation_id_in_file,
    int mutation_id, clang::Rewriter& rewriter) const {
  const clang::SourceRange lhs_source_range_in_main_file =
      GetSourceRangeInMainFile(preprocessor, *binary_operator_->getLHS());
  assert(lhs_source_range_in_main_file.isValid() && "Invalid source range.");
  const clang::SourceRange rhs_source_range_in_main_file =
      GetSourceRangeInMainFile(preprocessor, *binary_operator_->getRHS());
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
      binary_operator_->getOperatorLoc(),
      static_cast<unsigned int>(
          clang::BinaryOperator::getOpcodeStr(binary_operator_->getOpcode())
              .size()),
      ",");

  // These record the text that should be inserted before and after the LHS and
  // RHS operands.
  std::string lhs_prefix = new_function_name + "(";
  std::string lhs_suffix;
  std::string rhs_prefix;
  std::string rhs_suffix;

  if (ast_context.getLangOpts().CPlusPlus) {
    if (binary_operator_->getLHS()->HasSideEffects(ast_context)) {
      lhs_prefix.append("[&]() -> " + lhs_type + " { return static_cast<" +
                        lhs_type + ">(");
      lhs_suffix.append("); }");
    }
    if (binary_operator_->isLogicalOp() ||
        binary_operator_->getRHS()->HasSideEffects(ast_context)) {
      rhs_prefix.append("[&]() -> " + rhs_type + " { return static_cast<" +
                        rhs_type + ">(");
      rhs_suffix.append("); }");
    }
  } else {
    if (binary_operator_->isAssignmentOp()) {
      lhs_prefix.append("&(");
      lhs_suffix.append(")");
    }
  }
  rhs_suffix.append(", " + std::to_string(local_mutation_id) + ")");

  // The prefixes and suffixes are ready, so make the relevant insertions.
  bool rewriter_result = rewriter.InsertTextBefore(
      lhs_source_range_in_main_file.getBegin(), lhs_prefix);
  assert(!rewriter_result && "Rewrite failed.\n");
  rewriter_result = rewriter.InsertTextAfterToken(
      lhs_source_range_in_main_file.getEnd(), lhs_suffix);
  assert(!rewriter_result && "Rewrite failed.\n");
  rewriter_result = rewriter.InsertTextBefore(
      rhs_source_range_in_main_file.getBegin(), rhs_prefix);
  assert(!rewriter_result && "Rewrite failed.\n");
  rewriter_result = rewriter.InsertTextAfterToken(
      rhs_source_range_in_main_file.getEnd(), rhs_suffix);
  assert(!rewriter_result && "Rewrite failed.\n");
  (void)rewriter_result;  // Keep release-mode compilers happy.
}

void MutationReplaceBinaryOperator::HandleCLogicalOperator(
    const clang::Preprocessor& preprocessor,
    const std::string& new_function_prefix, const std::string& result_type,
    const std::string& lhs_type, const std::string& rhs_type,
    bool only_track_mutant_coverage, int first_mutation_id_in_file,
    int& mutation_id, clang::Rewriter& rewriter,
    std::unordered_set<std::string>& dredd_declarations) const {
  // A C logical operator "op" is handled by transforming:
  //
  //   a op b
  //
  // to:
  //
  //   __dredd_fun_outer(__dredd_fun_lhs(a) op __dredd_fun_rhs(b))
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

  if (!only_track_mutant_coverage) {
    {
      // Rewrite the LHS of the expression, and introduce the associated
      // function.
      auto source_range_lhs =
          GetSourceRangeInMainFile(preprocessor, *binary_operator_->getLHS());
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
      if (binary_operator_->getOpcode() == clang::BinaryOperatorKind::BO_LAnd) {
        // Replacing "a && b" with "b" is achieved by replacing "a" with "1".
        lhs_function
            << "  if (__dredd_enabled_mutation(local_mutation_id + 2)) "
               "return 1;\n";
      } else {
        // Replacing "a || b" with "b" is achieved by replacing "a" with "0".
        lhs_function
            << "  if (__dredd_enabled_mutation(local_mutation_id + 2)) "
               "return 0;\n";
      }
      lhs_function << "  return arg;\n";
      lhs_function << "}\n";
      dredd_declarations.insert(lhs_function.str());
    }

    {
      // Rewrite the RHS of the expression, and introduce the associated
      // function.
      auto source_range_rhs =
          GetSourceRangeInMainFile(preprocessor, *binary_operator_->getRHS());
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
      if (binary_operator_->getOpcode() == clang::BinaryOperatorKind::BO_LAnd) {
        // Replacing "a && b" with "a" is achieved by replacing "b" with "1".
        rhs_function
            << "  if (__dredd_enabled_mutation(local_mutation_id + 1)) "
               "return 1;\n";
      } else {
        // Replacing "a || b" with "a" is achieved by replacing "b" with "0".
        rhs_function
            << "  if (__dredd_enabled_mutation(local_mutation_id + 1)) "
               "return 0;\n";
      }

      // Case 2: replacing with RHS: no action is needed here.

      rhs_function << "  return arg;\n";
      rhs_function << "}\n";
      dredd_declarations.insert(rhs_function.str());
    }
  }

  {
    // Rewrite the overall expression, and introduce the associated function.
    auto source_range_binary_operator =
        GetSourceRangeInMainFile(preprocessor, *binary_operator_);
    const std::string outer_function_name = new_function_prefix + "_outer";
    rewriter.InsertTextBefore(source_range_binary_operator.getBegin(),
                              outer_function_name + "(");
    rewriter.InsertTextAfterToken(
        source_range_binary_operator.getEnd(),
        ", " + std::to_string(mutation_id - first_mutation_id_in_file) + ")");

    std::stringstream outer_function;
    outer_function << "static " << result_type << " " << outer_function_name
                   << "(" << result_type << " arg, int local_mutation_id) {\n";
    if (!only_track_mutant_coverage) {
      // Case 0: swapping the operator.
      // Replacing && with || is achieved by negating the whole expression, and
      // negating each of the LHS and RHS. The same holds for replacing || with
      // &&. This case handles negating the whole expression.
      outer_function
          << "  if (__dredd_enabled_mutation(local_mutation_id + 0)) "
             "return !arg;\n";

      // Case 1: replacing with LHS: no action is needed here.

      // Case 2: replacing with RHS: no action is needed here.
    }
    if (only_track_mutant_coverage) {
      // The fact that three mutants are covered is recorded, to reflect
      // swapping the operator, replacing with LHS and replacing with RHS.
      // It does not matter in which function this is recorded, but intuitively
      // it seems most elegant for the function enclosing the whole expression
      // to take care of it.
      outer_function
          << "  __dredd_record_covered_mutants(local_mutation_id, 3);\n";
    }
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

void MutationReplaceBinaryOperator::AddMutationInstance(
    int mutation_id_base, protobufs::MutationReplaceBinaryOperatorAction action,
    int& mutation_id_offset,
    protobufs::MutationReplaceBinaryOperator& protobuf_message) {
  protobufs::MutationReplaceBinaryOperatorInstance instance;
  instance.set_mutation_id(mutation_id_base + mutation_id_offset);
  instance.set_action(action);
  *protobuf_message.add_instances() = instance;
  mutation_id_offset++;
}

protobufs::MutationReplaceBinaryOperatorAction
MutationReplaceBinaryOperator::OperatorKindToAction(
    clang::BinaryOperatorKind operator_kind) {
  switch (operator_kind) {
    case clang::BinaryOperatorKind::BO_Add:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithAdd;
    case clang::BinaryOperatorKind::BO_Div:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithDiv;
    case clang::BinaryOperatorKind::BO_Mul:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithMul;
    case clang::BinaryOperatorKind::BO_Rem:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithRem;
    case clang::BinaryOperatorKind::BO_Sub:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithSub;
    case clang::BinaryOperatorKind::BO_AddAssign:
      return protobufs::MutationReplaceBinaryOperatorAction::
          ReplaceWithAddAssign;
    case clang::BinaryOperatorKind::BO_AndAssign:
      return protobufs::MutationReplaceBinaryOperatorAction::
          ReplaceWithAndAssign;
    case clang::BinaryOperatorKind::BO_Assign:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithAssign;
    case clang::BinaryOperatorKind::BO_DivAssign:
      return protobufs::MutationReplaceBinaryOperatorAction::
          ReplaceWithDivAssign;
    case clang::BinaryOperatorKind::BO_MulAssign:
      return protobufs::MutationReplaceBinaryOperatorAction::
          ReplaceWithMulAssign;
    case clang::BinaryOperatorKind::BO_OrAssign:
      return protobufs::MutationReplaceBinaryOperatorAction::
          ReplaceWithOrAssign;
    case clang::BinaryOperatorKind::BO_RemAssign:
      return protobufs::MutationReplaceBinaryOperatorAction::
          ReplaceWithRemAssign;
    case clang::BinaryOperatorKind::BO_ShlAssign:
      return protobufs::MutationReplaceBinaryOperatorAction::
          ReplaceWithShlAssign;
    case clang::BinaryOperatorKind::BO_ShrAssign:
      return protobufs::MutationReplaceBinaryOperatorAction::
          ReplaceWithShrAssign;
    case clang::BinaryOperatorKind::BO_SubAssign:
      return protobufs::MutationReplaceBinaryOperatorAction::
          ReplaceWithSubAssign;
    case clang::BinaryOperatorKind::BO_XorAssign:
      return protobufs::MutationReplaceBinaryOperatorAction::
          ReplaceWithXorAssign;
    case clang::BinaryOperatorKind::BO_And:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithAnd;
    case clang::BinaryOperatorKind::BO_Or:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithOr;
    case clang::BinaryOperatorKind::BO_Xor:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithXor;
    case clang::BinaryOperatorKind::BO_LAnd:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithLAnd;
    case clang::BinaryOperatorKind::BO_LOr:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithLOr;
    case clang::BinaryOperatorKind::BO_EQ:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithEQ;
    case clang::BinaryOperatorKind::BO_GE:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithGE;
    case clang::BinaryOperatorKind::BO_GT:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithGT;
    case clang::BinaryOperatorKind::BO_LE:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithLE;
    case clang::BinaryOperatorKind::BO_LT:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithLT;
    case clang::BinaryOperatorKind::BO_NE:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithNE;
    case clang::BinaryOperatorKind::BO_Shl:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithShl;
    case clang::BinaryOperatorKind::BO_Shr:
      return protobufs::MutationReplaceBinaryOperatorAction::ReplaceWithShr;
    default:
      assert(false && "Unknown operator kind.");
      return protobufs::MutationReplaceBinaryOperatorAction_MAX;
  }
}

protobufs::BinaryOperator
MutationReplaceBinaryOperator::ClangOperatorKindToProtobufOperatorKind(
    clang::BinaryOperatorKind operator_kind) {
  switch (operator_kind) {
    case clang::BinaryOperatorKind::BO_Mul:
      return protobufs::BinaryOperator::Mul;
    case clang::BinaryOperatorKind::BO_Div:
      return protobufs::BinaryOperator::Div;
    case clang::BinaryOperatorKind::BO_Rem:
      return protobufs::BinaryOperator::Rem;
    case clang::BinaryOperatorKind::BO_Add:
      return protobufs::BinaryOperator::Add;
    case clang::BinaryOperatorKind::BO_Sub:
      return protobufs::BinaryOperator::Sub;
    case clang::BinaryOperatorKind::BO_Shl:
      return protobufs::BinaryOperator::Shl;
    case clang::BinaryOperatorKind::BO_Shr:
      return protobufs::BinaryOperator::Shr;
    case clang::BinaryOperatorKind::BO_LT:
      return protobufs::BinaryOperator::LT;
    case clang::BinaryOperatorKind::BO_GT:
      return protobufs::BinaryOperator::GT;
    case clang::BinaryOperatorKind::BO_LE:
      return protobufs::BinaryOperator::LE;
    case clang::BinaryOperatorKind::BO_GE:
      return protobufs::BinaryOperator::GE;
    case clang::BinaryOperatorKind::BO_EQ:
      return protobufs::BinaryOperator::EQ;
    case clang::BinaryOperatorKind::BO_NE:
      return protobufs::BinaryOperator::NE;
    case clang::BinaryOperatorKind::BO_And:
      return protobufs::BinaryOperator::And;
    case clang::BinaryOperatorKind::BO_Xor:
      return protobufs::BinaryOperator::Xor;
    case clang::BinaryOperatorKind::BO_Or:
      return protobufs::BinaryOperator::Or;
    case clang::BinaryOperatorKind::BO_LAnd:
      return protobufs::BinaryOperator::LAnd;
    case clang::BinaryOperatorKind::BO_LOr:
      return protobufs::BinaryOperator::LOr;
    case clang::BinaryOperatorKind::BO_Assign:
      return protobufs::BinaryOperator::Assign;
    case clang::BinaryOperatorKind::BO_MulAssign:
      return protobufs::BinaryOperator::MulAssign;
    case clang::BinaryOperatorKind::BO_DivAssign:
      return protobufs::BinaryOperator::DivAssign;
    case clang::BinaryOperatorKind::BO_RemAssign:
      return protobufs::BinaryOperator::RemAssign;
    case clang::BinaryOperatorKind::BO_AddAssign:
      return protobufs::BinaryOperator::AddAssign;
    case clang::BinaryOperatorKind::BO_SubAssign:
      return protobufs::BinaryOperator::SubAssign;
    case clang::BinaryOperatorKind::BO_ShlAssign:
      return protobufs::BinaryOperator::ShlAssign;
    case clang::BinaryOperatorKind::BO_ShrAssign:
      return protobufs::BinaryOperator::ShrAssign;
    case clang::BinaryOperatorKind::BO_AndAssign:
      return protobufs::BinaryOperator::AndAssign;
    case clang::BinaryOperatorKind::BO_XorAssign:
      return protobufs::BinaryOperator::XorAssign;
    case clang::BinaryOperatorKind::BO_OrAssign:
      return protobufs::BinaryOperator::OrAssign;
    default:
      assert(false && "Unknown operator.");
      return protobufs::BinaryOperator_MAX;
  }
}

bool MutationReplaceBinaryOperator::
    IsRedundantReplacementForBooleanValuedOperator(
        clang::BinaryOperatorKind operator_kind) const {
  switch (binary_operator_->getOpcode()) {
    // From
    // https://people.cs.umass.edu/~rjust/publ/non_redundant_mutants_jstvr_2014.pdf:
    // For boolean operators, only a subset of replacements are non-redundant.
    case clang::BO_LAnd:
      return operator_kind != clang::BO_EQ;
    case clang::BO_LOr:
      return operator_kind != clang::BO_NE;
    case clang::BO_GT:
      return operator_kind != clang::BO_GE && operator_kind != clang::BO_NE;
    case clang::BO_GE:
      return operator_kind != clang::BO_GT && operator_kind != clang::BO_EQ;
    case clang::BO_LT:
      return operator_kind != clang::BO_LE && operator_kind != clang::BO_NE;
    case clang::BO_LE:
      return operator_kind != clang::BO_LT && operator_kind != clang::BO_EQ;
    case clang::BO_EQ:
      return operator_kind != clang::BO_LE && operator_kind != clang::BO_GE;
    case clang::BO_NE:
      return operator_kind != clang::BO_LT && operator_kind != clang::BO_GT;
    default:
      return false;
  }
}

bool MutationReplaceBinaryOperator::IsRedundantReplacementForArithmeticOperator(
    clang::BinaryOperatorKind operator_kind,
    const clang::ASTContext& ast_context) const {
  // In the case where both operands are 0, the only case that isn't covered
  // by constant replacement is undefined behaviour, this is achieved by /.
  if ((MutationReplaceExpr::ExprIsEquivalentToInt(*binary_operator_->getLHS(),
                                                  0, ast_context) ||
       MutationReplaceExpr::ExprIsEquivalentToFloat(*binary_operator_->getLHS(),
                                                    0.0, ast_context)) &&
      (MutationReplaceExpr::ExprIsEquivalentToInt(*binary_operator_->getRHS(),
                                                  0, ast_context) ||
       MutationReplaceExpr::ExprIsEquivalentToFloat(*binary_operator_->getRHS(),
                                                    0.0, ast_context))) {
    if (operator_kind == clang::BO_Div) {
      return false;
    }
  }

  // In the following cases, the replacement is equivalent to either replacement
  // with a constant or argument replacement.
  if ((MutationReplaceExpr::ExprIsEquivalentToInt(*binary_operator_->getRHS(),
                                                  0, ast_context) ||
       MutationReplaceExpr::ExprIsEquivalentToFloat(*binary_operator_->getRHS(),
                                                    0.0, ast_context))) {
    // When the right operand is 0: +, -, << and >> are all equivalent to
    // replacement with the right operand; * is equivalent to replacement with
    // the constant 0 and % is equivalent to replacement with / in that both
    // cases lead to undefined behaviour.
    if (operator_kind == clang::BO_Add || operator_kind == clang::BO_Sub ||
        operator_kind == clang::BO_Shl || operator_kind == clang::BO_Shr ||
        operator_kind == clang::BO_Mul || operator_kind == clang::BO_Rem) {
      return true;
    }
  }

  if ((MutationReplaceExpr::ExprIsEquivalentToInt(*binary_operator_->getRHS(),
                                                  1, ast_context) ||
       MutationReplaceExpr::ExprIsEquivalentToFloat(*binary_operator_->getRHS(),
                                                    1.0, ast_context))) {
    // When the right operand is 1: * and / are equivalent to replacement by
    // the left operand.
    if (operator_kind == clang::BO_Mul || operator_kind == clang::BO_Div) {
      return true;
    }
  }

  if ((MutationReplaceExpr::ExprIsEquivalentToInt(*binary_operator_->getLHS(),
                                                  0, ast_context) ||
       MutationReplaceExpr::ExprIsEquivalentToFloat(*binary_operator_->getLHS(),
                                                    0.0, ast_context))) {
    // When the left operand is 0: *, /, %, << and >> are equivalent to
    // replacement by the constant 0 and + is equivalent to replacement by the
    // right operand.
    if (operator_kind == clang::BO_Add || operator_kind == clang::BO_Shl ||
        operator_kind == clang::BO_Shr || operator_kind == clang::BO_Mul ||
        operator_kind == clang::BO_Rem || operator_kind == clang::BO_Div) {
      return true;
    }
  }

  if ((MutationReplaceExpr::ExprIsEquivalentToInt(*binary_operator_->getLHS(),
                                                  1, ast_context) ||
       MutationReplaceExpr::ExprIsEquivalentToFloat(*binary_operator_->getLHS(),
                                                    1.0, ast_context)) &&
      operator_kind == clang::BO_Mul) {
    // When the left operand is 1: * is equivalent to replacement by the right
    // operand.
    return true;
  }

  return false;
}

}  // namespace dredd
