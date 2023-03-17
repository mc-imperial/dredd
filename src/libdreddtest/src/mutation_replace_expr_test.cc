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

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>

#include "clang/AST/Expr.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersInternal.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Rewrite/Core/RewriteBuffer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/Tooling.h"
#include "libdreddtest/gtest.h"
#include "llvm/ADT/SmallVector.h"

namespace dredd {
namespace {

void TestReplacement(const std::string& original, const std::string& expected,
                     int num_replacements,
                     const std::string& expected_dredd_declaration,
                     const uint64_t expression_to_replace) {
  auto ast_unit = clang::tooling::buildASTFromCodeWithArgs(original, {"-w"});
  ASSERT_FALSE(ast_unit->getDiagnostics().hasErrorOccurred());
  auto function_decl = clang::ast_matchers::match(
      clang::ast_matchers::functionDecl(clang::ast_matchers::hasName("foo"))
          .bind("fn"),
      ast_unit->getASTContext());
  ASSERT_EQ(1, function_decl.size());

  auto expression = clang::ast_matchers::match(
      clang::ast_matchers::expr().bind("expr"), ast_unit->getASTContext());
  ASSERT_GT(expression.size(), 0);

  ASSERT_LT(expression_to_replace, expression.size());
  const MutationReplaceExpr mutation(
      *expression[expression_to_replace].getNodeAs<clang::Expr>("expr"),
      ast_unit->getPreprocessor(), ast_unit->getASTContext());

  clang::Rewriter rewriter(ast_unit->getSourceManager(),
                           ast_unit->getLangOpts());
  int mutation_id = 0;
  std::unordered_set<std::string> dredd_declarations;
  mutation.Apply(ast_unit->getASTContext(), ast_unit->getPreprocessor(), true,
                 false, 0, mutation_id, rewriter, dredd_declarations);
  ASSERT_EQ(num_replacements, mutation_id);
  ASSERT_EQ(1, dredd_declarations.size());
  ASSERT_EQ(expected_dredd_declaration, *dredd_declarations.begin());

  const clang::RewriteBuffer* rewrite_buffer = rewriter.getRewriteBufferFor(
      ast_unit->getSourceManager().getMainFileID());
  const std::string rewritten_text(rewrite_buffer->begin(),
                                   rewrite_buffer->end());
  ASSERT_EQ(expected, rewritten_text);
}

TEST(MutationReplaceExprTest, MutateSignedConstants) {
  const std::string original = "void foo() { 2; }";
  const std::string expected =
      "void foo() { __dredd_replace_expr_int_constant([&]() -> int { "
      "return 2; }, 0); }";
  const std::string expected_dredd_declaration =
      R"(static int __dredd_replace_expr_int_constant(std::function<int()> arg, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return ~(arg());
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return -(arg());
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return 0;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return 1;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return -1;
  return arg();
}

)";
  const int kNumReplacements = 5;
  TestReplacement(original, expected, kNumReplacements,
                  expected_dredd_declaration, 0);
}

TEST(MutationReplaceExprTest, MutateUnsignedConstants) {
  const std::string original = "void foo() { unsigned int x = 2; }";
  const std::string expected =
      "void foo() { unsigned int x = "
      "__dredd_replace_expr_unsigned_int_constant([&]() "
      "-> unsigned int { "
      "return 2; }, 0); }";
  const std::string expected_dredd_declaration =
      R"(static unsigned int __dredd_replace_expr_unsigned_int_constant(std::function<unsigned int()> arg, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return ~(arg());
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return 0;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return 1;
  return arg();
}

)";
  const int kNumReplacements = 3;
  TestReplacement(original, expected, kNumReplacements,
                  expected_dredd_declaration, 0);
}

TEST(MutationReplaceExprTest, MutateFloatConstants) {
  const std::string original = "void foo() { 2.523; }";
  const std::string expected =
      "void foo() { __dredd_replace_expr_double([&]() -> double { "
      "return 2.523; }, 0); }";
  const std::string expected_dredd_declaration =
      R"(static double __dredd_replace_expr_double(std::function<double()> arg, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return -(arg());
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return 0.0;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return 1.0;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return -1.0;
  return arg();
}

)";
  const int kNumReplacements = 4;
  TestReplacement(original, expected, kNumReplacements,
                  expected_dredd_declaration, 0);
}

TEST(MutationReplaceExprTest, MutateLValues) {
  const std::string original =
      R"(void foo() {
  int x;
  -x;
}
)";
  const std::string expected =
      R"(void foo() {
  int x;
  -__dredd_replace_expr_int_lvalue([&]() -> int& { return static_cast<int&>(x); }, 0);
}
)";
  const std::string expected_dredd_declaration =
      R"(static int __dredd_replace_expr_int_lvalue(std::function<int&()> arg, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return ++(arg());
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return --(arg());
  return arg();
}

)";
  const int kNumReplacements = 2;
  TestReplacement(original, expected, kNumReplacements,
                  expected_dredd_declaration, 2);
}

TEST(MutationReplaceExprTest, MutateFunctionArgs) {
  const std::string original =
      R"(
int neg(int x);

void foo() {
  int x;
  neg(x);
}

int neg(int x) {
  return -x;
}
)";
  const std::string expected =
      R"(
int neg(int x);

void foo() {
  int x;
  neg(__dredd_replace_expr_int([&]() -> int { return static_cast<int>(x); }, 0));
}

int neg(int x) {
  return -x;
}
)";
  const std::string expected_dredd_declaration =
      R"(static int __dredd_replace_expr_int(std::function<int()> arg, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return !(arg());
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return ~(arg());
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return -(arg());
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return 0;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return 1;
  if (__dredd_enabled_mutation(local_mutation_id + 5)) return -1;
  return arg();
}

)";
  const int kNumReplacements = 6;
  TestReplacement(original, expected, kNumReplacements,
                  expected_dredd_declaration, 2);
}

TEST(MutationReplaceExprTest, MutateLAnd) {
  const std::string original =
      R"(
bool foo(bool a, bool b) {
  return a && b;
}
)";
  const std::string expected =
      R"(
bool foo(bool a, bool b) {
  return __dredd_replace_expr_bool_omit_true([&]() -> bool { return static_cast<bool>(a && b); }, 0);
}
)";
  const std::string expected_dredd_declaration =
      R"(static bool __dredd_replace_expr_bool_omit_true(std::function<bool()> arg, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return false;
  return arg();
}

)";
  const int kNumReplacements = 1;
  TestReplacement(original, expected, kNumReplacements,
                  expected_dredd_declaration, 0);
}

TEST(MutationReplaceExprTest, MutateLOr) {
  const std::string original =
      R"(
bool foo(bool a, bool b) {
  return a || b;
}
)";
  const std::string expected =
      R"(
bool foo(bool a, bool b) {
  return __dredd_replace_expr_bool_omit_false([&]() -> bool { return static_cast<bool>(a || b); }, 0);
}
)";
  const std::string expected_dredd_declaration =
      R"(static bool __dredd_replace_expr_bool_omit_false(std::function<bool()> arg, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return true;
  return arg();
}

)";
  const int kNumReplacements = 1;
  TestReplacement(original, expected, kNumReplacements,
                  expected_dredd_declaration, 0);
}

}  // namespace
}  // namespace dredd
