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

#include "clang/AST/Expr.h"
#include "libdredd/util.h"

namespace dredd {
dredd::MutationReplaceExpr::MutationReplaceExpr(const clang::Expr& expr)
    : expr_(expr) {}

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
  if (exprType->isBooleanType() ||
      (exprType->isInteger() && !exprType->isBooleanType() &&
       !expr_.isLValue())) {
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
  std::string new_function_name = "test";
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
  if (ast_context.getLangOpts().CPlusPlus) {
    bool result = rewriter.ReplaceText(
        expr_source_range_in_main_file,
        new_function_name + "([&]() -> " + result_type + " { return " +
            // We don't need to static cast constant expressions
            (expr_.isCXX11ConstantExpr(ast_context)
                 ? rewriter.getRewrittenText(expr_source_range_in_main_file)
                 : ("static_cast<" + result_type + ">(" +
                    rewriter.getRewrittenText(expr_source_range_in_main_file) +
                    ")")) +
            "; }" + ", " + std::to_string(local_mutation_id) + ")");
    (void)result;  // Keep release-mode compilers happy.
    assert(!result && "Rewrite failed.\n");
  } else {
    std::string input_arg =
        rewriter.getRewrittenText(expr_source_range_in_main_file);
    bool result =
        rewriter.ReplaceText(expr_source_range_in_main_file,
                             new_function_name + "(" + input_arg + ", " +
                                 std::to_string(local_mutation_id) + ")");
    (void)result;  // Keep release-mode compilers happy.
    assert(!result && "Rewrite failed.\n");
  }

  std::string new_function = GenerateMutatorFunction(
      ast_context, new_function_name, result_type, mutation_id);
  assert(!new_function.empty() && "Unsupported expression.");

  dredd_declarations.insert(new_function);
}

}  // namespace dredd
