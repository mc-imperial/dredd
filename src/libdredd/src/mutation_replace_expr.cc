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

#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Type.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/util.h"
#include "llvm/ADT/StringRef.h"

namespace dredd {
dredd::MutationReplaceExpr::MutationReplaceExpr(const clang::Expr& expr)
    : expr_(expr) {}

std::string MutationReplaceExpr::GetFunctionName() const {
  std::string result = "__dredd_replace_expr_";

  // A string corresponding to the expression forms part of the name of the
  // mutation function, to differentiate mutation functions for different
  // types
  if (expr_.getType()->isBooleanType()) {
    result += "Bool";
  } else if (expr_.getType()->isIntegerType()) {
    result += "Int";
  } else if (expr_.getType()->isFloatingType()) {
    result += "Float";
  } else {
    assert(false && "Unsupported opcode");
  }

  return result;
}

std::string MutationReplaceExpr::GenerateMutatorFunction(
    clang::ASTContext& ast_context, const std::string& function_name,
    const std::string& result_type, int& mutation_id) const {
  std::stringstream new_function;
  new_function << "static " << result_type << " " << function_name << "(";
  if (ast_context.getLangOpts().CPlusPlus) {
    new_function << "std::function<" << result_type << "()>";
  } else {
    new_function << result_type;
  }
  new_function << " arg, int local_mutation_id) {\n";

  std::string arg_evaluated = "arg";
  if (ast_context.getLangOpts().CPlusPlus) {
    arg_evaluated += "()";
  }

  // Quickly apply the original operator if no mutant is enabled (which will be
  // the common case).
  new_function << "  if (!__dredd_some_mutation_enabled) return "
               << arg_evaluated << ";\n";

  int mutant_offset = 0;

  const clang::BuiltinType* exprType =
      expr_.getType()->getAs<clang::BuiltinType>();
  if (!expr_.isLValue() &&
      (exprType->isBooleanType() || exprType->isInteger())) {
    new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                 << mutant_offset << ")) return !(" << arg_evaluated << ");\n";
    mutant_offset++;
  }

  // Insert constants for integer expressions
  if (!expr_.isLValue()) {
    if (exprType->isInteger() && !exprType->isBooleanType()) {
      // Replace expression with 0
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return 0;\n";
      mutant_offset++;

      // Replace expression with 1
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return 1;\n";
      mutant_offset++;
    }

    if (exprType->isSignedInteger()) {
      // Replace signed integer expression with -1
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return -1;\n";
      mutant_offset++;
    }

    if (exprType->isFloatingPoint()) {
      // Replace floating point expression with 0.0
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return 0.0;\n";
      mutant_offset++;

      // Replace floating point expression with 1.0
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return 1.0;\n";
      mutant_offset++;

      // Replace floating point expression with -1.0
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return -1.0;\n";
      mutant_offset++;
    }
  }

  new_function << "  return " << arg_evaluated << ";\n";
  new_function << "}\n\n";

  mutation_id += mutant_offset;

  return new_function.str();
}

void MutationReplaceExpr::Apply(
    clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
    int first_mutation_id_in_file, int& mutation_id, clang::Rewriter& rewriter,
    std::unordered_set<std::string>& dredd_declarations) const {
  std::string new_function_name = GetFunctionName();
  std::string result_type = expr_.getType()
                                ->getAs<clang::BuiltinType>()
                                ->getName(ast_context.getPrintingPolicy())
                                .str();

  clang::SourceRange expr_source_range_in_main_file =
      GetSourceRangeInMainFile(preprocessor, expr_);
  assert(expr_source_range_in_main_file.isValid() && "Invalid source range.");

  // Replace the unary operator expression with a call to the wrapper
  // function.
  //
  // Subtracting |first_mutation_id_in_file| turns the global mutation id,
  // |mutation_id|, into a file-local mutation id.
  const int local_mutation_id = mutation_id - first_mutation_id_in_file;

  // Replacement of an expression with a function call is simulated by
  // Inserting suitable text before and after the expression.
  // This is preferable over the (otherwise more intuitive) approach of directly
  // replacing the text for the unary operator node, because the Clang rewriter
  // does not support nested replacements.

  // These record the text that should be inserted before and after the operand.
  std::string prefix;
  std::string suffix;
  if (ast_context.getLangOpts().CPlusPlus) {
    prefix = new_function_name + "([&]() -> " + result_type + " { return " +
             // We don't need to static cast constant expressions
             (expr_.isCXX11ConstantExpr(ast_context)
                  ? ""
                  : "static_cast<" + result_type + ">(");
    suffix = (expr_.isCXX11ConstantExpr(ast_context) ? "" : ")");
    suffix.append("; }, " + std::to_string(local_mutation_id) + ")");
  } else {
    prefix = new_function_name + "(";
    suffix = ", " + std::to_string(local_mutation_id) + ")";
  }

  bool result = rewriter.InsertTextBefore(
      expr_source_range_in_main_file.getBegin(), prefix);
  assert(!result && "Rewrite failed.\n");
  result = rewriter.InsertTextAfterToken(
      expr_source_range_in_main_file.getEnd(), suffix);
  assert(!result && "Rewrite failed.\n");
  (void)result;

  std::string new_function = GenerateMutatorFunction(
      ast_context, new_function_name, result_type, mutation_id);
  assert(!new_function.empty() && "Unsupported expression.");

  dredd_declarations.insert(new_function);
}

}  // namespace dredd
