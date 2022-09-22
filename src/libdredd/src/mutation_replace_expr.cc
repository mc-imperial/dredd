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
dredd::MutationReplaceExpr::MutationReplaceExpr(const clang::Expr& expr)
    : expr_(expr) {}

std::string MutationReplaceExpr::GetFunctionName(
    bool optimise_mutations, clang::ASTContext& ast_context) const {
  std::string result = "__dredd_replace_expr_";

  if (expr_.isLValue()) {
    clang::QualType qualified_type = expr_.getType();
    if (qualified_type.isVolatileQualified()) {
      assert(expr_.getType().isVolatileQualified() &&
             "Expected expression to be volatile-qualified since subexpression "
             "is.");
      result += "volatile_";
    }
  }

  // A string corresponding to the expression forms part of the name of the
  // mutation function, to differentiate mutation functions for different
  // types
  result += SpaceToUnderscore(expr_.getType()
                                  ->getAs<clang::BuiltinType>()
                                  ->getName(ast_context.getPrintingPolicy())
                                  .str());

  if (expr_.isLValue()) {
    result += "_lvalue";
  }

  if (optimise_mutations) {
    AddOptimisationSpecifier(ast_context, result);
  }

  return result;
}

void MutationReplaceExpr::AddOptimisationSpecifier(
    clang::ASTContext& ast_context,
    std::string& function_name)
    const {  // This is sufficient to prevent name clashes as this is the
             // only unary operator insertion optimisation that depends on the
             // input expression. ~ is omitted for all boolean expressions
             // so the type that is included in the function name will be enough
             // in that case.
  if (IsRedundantOperatorInsertion(expr_, ast_context)) {
    function_name += "_uoi_optimised";
  }

  if ((expr_.getType()->isIntegerType() && !expr_.getType()->isBooleanType()) ||
      expr_.getType()->isFloatingType()) {
    if (ExprIsEquivalentToInt(expr_, 0, ast_context) ||
        ExprIsEquivalentToFloat(expr_, 0, ast_context)) {
      function_name += "_zero";
    } else if (ExprIsEquivalentToInt(expr_, 1, ast_context) ||
               ExprIsEquivalentToFloat(expr_, 1, ast_context)) {
      function_name += "_one";
    } else if (ExprIsEquivalentToInt(expr_, -1, ast_context) ||
               ExprIsEquivalentToFloat(expr_, -1, ast_context)) {
      function_name += "_minus_one";
    }
  }

  if (expr_.getType()->isBooleanType()) {
    if (ExprIsEquivalentToBool(expr_, true, ast_context)) {
      function_name += "_true";
    } else if (ExprIsEquivalentToBool(expr_, false, ast_context)) {
      function_name += "_false";
    }
  }
}

bool MutationReplaceExpr::ExprIsEquivalentToInt(
    const clang::Expr& expr, int constant, clang::ASTContext& ast_context) {
  clang::Expr::EvalResult int_eval_result;
  if (expr.getType()->isIntegerType() &&
      expr.EvaluateAsInt(int_eval_result, ast_context)) {
    return llvm::APSInt::isSameValue(int_eval_result.Val.getInt(),
                                     llvm::APSInt::get(constant));
  }

  return false;
}

bool MutationReplaceExpr::ExprIsEquivalentToFloat(
    const clang::Expr& expr, double constant, clang::ASTContext& ast_context) {
  llvm::APFloat float_eval_result(static_cast<double>(0));
  if (expr.getType()->isFloatingType() &&
      expr.EvaluateAsFloat(float_eval_result, ast_context)) {
    return float_eval_result.isExactlyValue(constant);
  }

  return false;
}

bool MutationReplaceExpr::ExprIsEquivalentToBool(
    const clang::Expr& expr, bool constant, clang::ASTContext& ast_context) {
  bool bool_eval_result = false;
  if (expr.getType()->isBooleanType() &&
      expr.EvaluateAsBooleanCondition(bool_eval_result, ast_context)) {
    return bool_eval_result == constant;
  }

  return false;
}

bool MutationReplaceExpr::IsRedundantOperatorInsertion(
    const clang::Expr& expr, clang::ASTContext& ast_context) {
  // We use this value to capture the value that the expression evaluates
  // to if it does indeed evaluate to a constant, but we do not use this value
  // because either way, operator insertion would be redundant.
  bool unused_bool_value;
  return (expr.getType()->isBooleanType() &&
          expr.EvaluateAsBooleanCondition(unused_bool_value, ast_context)) ||
         (expr.getType()->isIntegerType() &&
          (ExprIsEquivalentToInt(expr, 0, ast_context) ||
           ExprIsEquivalentToInt(expr, 1, ast_context)));
}

void MutationReplaceExpr::GenerateUnaryOperatorInsertion(
    const std::string& arg_evaluated, clang::ASTContext& ast_context,
    bool optimise_mutations, std::stringstream& new_function,
    int& mutant_offset) const {
  const clang::BuiltinType& exprType =
      *expr_.getType()->getAs<clang::BuiltinType>();

  if (expr_.isLValue() && CanMutateLValue(ast_context, expr_)) {
    new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                 << mutant_offset << ")) return ++(" << arg_evaluated << ");\n";
    mutant_offset++;

    new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                 << mutant_offset << ")) return --(" << arg_evaluated << ");\n";
    mutant_offset++;
  }

  if (!expr_.isLValue() && (exprType.isBooleanType() || exprType.isInteger())) {
    if (!optimise_mutations ||
        !IsRedundantOperatorInsertion(expr_, ast_context)) {
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return !(" << arg_evaluated
                   << ");\n";
      mutant_offset++;
    }

    if (!expr_.getType()->isBooleanType()) {
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return ~(" << arg_evaluated
                   << ");\n";
      mutant_offset++;
    }
  }
}

void MutationReplaceExpr::GenerateConstantReplacement(
    clang::ASTContext& ast_context, bool optimise_mutations,
    std::stringstream& new_function, int& mutant_offset) const {
  if (!expr_.isLValue()) {
    GenerateBooleanConstantReplacement(ast_context, optimise_mutations,
                                       new_function, mutant_offset);
    GenerateIntegerConstantReplacement(ast_context, optimise_mutations,
                                       new_function, mutant_offset);
    GenerateFloatConstantReplacement(ast_context, optimise_mutations,
                                     new_function, mutant_offset);
  }
}
void MutationReplaceExpr::GenerateFloatConstantReplacement(
    clang::ASTContext& ast_context, bool optimise_mutations,
    std::stringstream& new_function, int& mutant_offset) const {
  const clang::BuiltinType& exprType =
      *expr_.getType()->getAs<clang::BuiltinType>();
  if (exprType.isFloatingPoint()) {
    if (!optimise_mutations ||
        !ExprIsEquivalentToFloat(expr_, 0, ast_context)) {
      // Replace floating point expression with 0.0
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return 0.0;\n";
      mutant_offset++;
    }

    if (!optimise_mutations ||
        !ExprIsEquivalentToFloat(expr_, 1, ast_context)) {
      // Replace floating point expression with 1.0
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return 1.0;\n";
      mutant_offset++;
    }

    if (!optimise_mutations ||
        !ExprIsEquivalentToFloat(expr_, -1, ast_context)) {
      // Replace floating point expression with -1.0
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return -1.0;\n";
      mutant_offset++;
    }
  }
}
void MutationReplaceExpr::GenerateIntegerConstantReplacement(
    clang::ASTContext& ast_context, bool optimise_mutations,
    std::stringstream& new_function, int& mutant_offset) const {
  const clang::BuiltinType& exprType =
      *expr_.getType()->getAs<clang::BuiltinType>();
  if (exprType.isInteger() && !exprType.isBooleanType()) {
    if (!optimise_mutations || !ExprIsEquivalentToInt(expr_, 0, ast_context)) {
      // Replace expression with 0
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return 0;\n";
      mutant_offset++;
    }

    if (!optimise_mutations || !ExprIsEquivalentToInt(expr_, 1, ast_context)) {
      // Replace expression with 1
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return 1;\n";
      mutant_offset++;
    }
  }

  if (exprType.isSignedInteger()) {
    if (!optimise_mutations || !ExprIsEquivalentToInt(expr_, -1, ast_context)) {
      // Replace signed integer expression with -1
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return -1;\n";
      mutant_offset++;
    }
  }
}
void MutationReplaceExpr::GenerateBooleanConstantReplacement(
    clang::ASTContext& ast_context, bool optimise_mutations,
    std::stringstream& new_function, int& mutant_offset) const {
  const clang::BuiltinType& exprType =
      *expr_.getType()->getAs<clang::BuiltinType>();
  if (exprType.isBooleanType()) {
    if (!optimise_mutations ||
        !ExprIsEquivalentToBool(expr_, true, ast_context)) {
      // Replace expression with true
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return "
                   << (ast_context.getLangOpts().CPlusPlus ? "true" : "1")
                   << ";\n";
      mutant_offset++;
    }

    if (!optimise_mutations ||
        !ExprIsEquivalentToBool(expr_, false, ast_context)) {
      // Replace expression with false
      new_function << "  if (__dredd_enabled_mutation(local_mutation_id + "
                   << mutant_offset << ")) return "
                   << (ast_context.getLangOpts().CPlusPlus ? "false" : "0")
                   << ";\n";
      mutant_offset++;
    }
  }
}

std::string MutationReplaceExpr::GenerateMutatorFunction(
    clang::ASTContext& ast_context, const std::string& function_name,
    const std::string& result_type, const std::string& input_type,
    bool optimise_mutations, int& mutation_id) const {
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
  } else if (expr_.isLValue()) {
    arg_evaluated = "(*" + arg_evaluated + ")";
  }

  // Quickly apply the original operator if no mutant is enabled (which will be
  // the common case).
  new_function << "  if (!__dredd_some_mutation_enabled) return "
               << arg_evaluated << ";\n";

  int mutant_offset = 0;

  GenerateUnaryOperatorInsertion(arg_evaluated, ast_context, optimise_mutations,
                                 new_function, mutant_offset);
  GenerateConstantReplacement(ast_context, optimise_mutations, new_function,
                              mutant_offset);

  new_function << "  return " << arg_evaluated << ";\n";
  new_function << "}\n\n";

  mutation_id += mutant_offset;

  return new_function.str();
}

void MutationReplaceExpr::ApplyCppTypeModifiers(const clang::Expr* expr,
                                                std::string& type) {
  if (expr->isLValue()) {
    type += "&";
    clang::QualType qualified_type = expr->getType();
    if (qualified_type.isVolatileQualified()) {
      type = "volatile " + type;
    } else if (qualified_type.isConstQualified()) {
      type = "const " + type;
    }
  }
}

void MutationReplaceExpr::ApplyCTypeModifiers(const clang::Expr* expr,
                                              std::string& type) {
  if (expr->isLValue()) {
    type += "*";
    clang::QualType qualified_type = expr->getType();
    if (qualified_type.isVolatileQualified()) {
      type = "volatile " + type;
    } else if (qualified_type.isConstQualified()) {
      type += "const " + type;
    }
  }
}

void MutationReplaceExpr::Apply(
    clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
    bool optimise_mutations, int first_mutation_id_in_file, int& mutation_id,
    clang::Rewriter& rewriter,
    std::unordered_set<std::string>& dredd_declarations) const {
  (void)optimise_mutations;  // Unused

  std::string new_function_name =
      GetFunctionName(optimise_mutations, ast_context);
  std::string result_type = expr_.getType()
                                ->getAs<clang::BuiltinType>()
                                ->getName(ast_context.getPrintingPolicy())
                                .str();

  std::string input_type = result_type;
  // Type modifiers are added to the input type, if it is an l-value. The result
  // type is left unmodified, because l-values are only mutated in positions
  // where they are implicitly cast to r-values, so the associated mutator
  // function should return a value, not a reference.
  if (ast_context.getLangOpts().CPlusPlus) {
    ApplyCppTypeModifiers(&expr_, input_type);
  } else {
    ApplyCTypeModifiers(&expr_, input_type);
  }

  clang::SourceRange expr_source_range_in_main_file =
      GetSourceRangeInMainFile(preprocessor, expr_);
  assert(expr_source_range_in_main_file.isValid() && "Invalid source range.");

  // Replace the operator expression with a call to the wrapper function.
  //
  // Subtracting |first_mutation_id_in_file| turns the global mutation id,
  // |mutation_id|, into a file-local mutation id.
  const int local_mutation_id = mutation_id - first_mutation_id_in_file;

  // Replacement of an expression with a function call is simulated by
  // Inserting suitable text before and after the expression.
  // This is preferable over the (otherwise more intuitive) approach of directly
  // replacing the text for the expression node, because the Clang rewriter
  // does not support nested replacements.

  // These record the text that should be inserted before and after the
  // expression.
  std::string prefix;
  std::string suffix;

  if (ast_context.getLangOpts().CPlusPlus) {
    prefix.append(new_function_name + "([&]() -> " + input_type + " { return " +
                  // We don't need to static cast constant expressions
                  (expr_.isCXX11ConstantExpr(ast_context)
                       ? ""
                       : "static_cast<" + input_type + ">("));
    suffix.append(expr_.isCXX11ConstantExpr(ast_context) ? "" : ")");
    suffix.append("; }");
  } else {
    prefix.append(new_function_name + "(");
    if (expr_.isLValue() && input_type.ends_with('*')) {
      prefix.append("&(");
      suffix.append(")");
    } else if (const auto* binary_expr =
                   llvm::dyn_cast<clang::BinaryOperator>(&expr_)) {
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
  auto parents = ast_context.getParents<clang::Expr>(expr_);
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

  bool result = rewriter.InsertTextBefore(
      expr_source_range_in_main_file.getBegin(), prefix);
  assert(!result && "Rewrite failed.\n");
  result = rewriter.InsertTextAfterToken(
      expr_source_range_in_main_file.getEnd(), suffix);
  assert(!result && "Rewrite failed.\n");
  (void)result;  // Keep release mode compilers happy.

  std::string new_function =
      GenerateMutatorFunction(ast_context, new_function_name, result_type,
                              input_type, optimise_mutations, mutation_id);
  assert(!new_function.empty() && "Unsupported expression.");

  dredd_declarations.insert(new_function);
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

}  // namespace dredd
