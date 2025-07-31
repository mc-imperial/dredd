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

#include <memory>
#include <string>
#include <vector>

#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
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
                     int num_replacements, bool optimise_mutations,
                     const std::string& expected_dredd_declaration) {
  auto ast_unit = clang::tooling::buildASTFromCodeWithArgs(original, {"-w"});
  ASSERT_FALSE(ast_unit->getDiagnostics().hasErrorOccurred());
  auto function_decl = clang::ast_matchers::match(
      clang::ast_matchers::functionDecl(clang::ast_matchers::hasName("foo"))
          .bind("fn"),
      ast_unit->getASTContext());
  ASSERT_EQ(1, function_decl.size());

  auto unary_operator = clang::ast_matchers::match(
      clang::ast_matchers::unaryOperator().bind("op"),
      ast_unit->getASTContext());
  ASSERT_EQ(1, unary_operator.size());

  const MutationReplaceUnaryOperator mutation(
      *unary_operator[0].getNodeAs<clang::UnaryOperator>("op"),
      ast_unit->getPreprocessor(), ast_unit->getASTContext());

  clang::Rewriter rewriter(ast_unit->getSourceManager(),
                           ast_unit->getLangOpts());
  int mutation_id = 0;
  std::unordered_set<std::string> dredd_declarations;
  mutation.Apply(ast_unit->getASTContext(), ast_unit->getPreprocessor(),
                 Options(optimise_mutations, false, false, false, false), 0,
                 mutation_id, rewriter, dredd_declarations);
  ASSERT_EQ(num_replacements, mutation_id);
  ASSERT_EQ(1, dredd_declarations.size());
  ASSERT_EQ(expected_dredd_declaration, *dredd_declarations.begin());

  const clang::RewriteBuffer* rewrite_buffer = rewriter.getRewriteBufferFor(
      ast_unit->getSourceManager().getMainFileID());
  const std::string rewritten_text(rewrite_buffer->begin(),
                                   rewrite_buffer->end());
  ASSERT_EQ(expected, rewritten_text);
}

TEST(MutationReplaceUnaryOperatorTest, MutateMinus) {
  const std::string original = "void foo() { -2; }";
  const std::string expected_opt =
      "void foo() { __dredd_replace_unary_operator_Minus_int(2, 0); }";
  const std::string expected_dredd_declaration_opt =
      R"(static int __dredd_replace_unary_operator_Minus_int(int arg, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return -arg;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return ~arg;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return !arg;
  return -arg;
}

)";
  const int kNumReplacementsOpt = 2;

  // Test with optimisations
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      "void foo() { __dredd_replace_unary_operator_Minus_int(2, 0); }";
  const std::string expected_dredd_declaration_no_opt =
      R"(static int __dredd_replace_unary_operator_Minus_int(int arg, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return -arg;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return ~arg;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return !arg;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg;
  return -arg;
}

)";
  const int kNumReplacementsNoOpt = 3;

  // Test without optimisations
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceUnaryOperatorTest, MutateNot) {
  const std::string original = R"(void foo() {
  bool f = false;
  !f;
}
)";
  const std::string expected_opt =
      R"(void foo() {
  bool f = false;
  __dredd_replace_unary_operator_LNot_bool(f, 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static bool __dredd_replace_unary_operator_LNot_bool(bool arg, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return !arg;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return ~arg;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return -arg;
  return !arg;
}

)";
  const int kNumReplacementsOpt = 2;

  // Test with optimisations
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo() {
  bool f = false;
  __dredd_replace_unary_operator_LNot_bool(f, 0);
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static bool __dredd_replace_unary_operator_LNot_bool(bool arg, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return !arg;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return ~arg;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return -arg;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg;
  return !arg;
}

)";
  const int kNumReplacementsNoOpt = 3;

  // Test without optimisations
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceUnaryOperatorTest, MutateIncrement) {
  const std::string original = R"(void foo() {
  double x = 5.364;
  ++x;
}
)";
  const std::string expected_opt =
      R"(void foo() {
  double x = 5.364;
  __dredd_replace_unary_operator_PreInc_double([&]() -> double& { return static_cast<double&>(x); }, 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static double& __dredd_replace_unary_operator_PreInc_double(std::function<double&()> arg, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return ++arg();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return --arg();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg();
  return ++arg();
}

)";
  const int kNumReplacementsOpt = 2;

  // Test with optimisations
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo() {
  double x = 5.364;
  __dredd_replace_unary_operator_PreInc_double([&]() -> double& { return static_cast<double&>(x); }, 0);
}
)";
  const std::string expected_dredd_declaration_noopt =
      R"(static double& __dredd_replace_unary_operator_PreInc_double(std::function<double&()> arg, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return ++arg();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return --arg();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg();
  return ++arg();
}

)";
  const int kNumReplacementsNoOpt = 2;

  // Test without optimisations
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_noopt);
}

TEST(MutationReplaceUnaryOperatorTest, MutateDecrement) {
  const std::string original = R"(void foo() {
  int x = 2;
  x--;
}
)";
  const std::string expected_opt =
      R"(void foo() {
  int x = 2;
  __dredd_replace_unary_operator_PostDec_int([&]() -> int& { return static_cast<int&>(x); }, 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static int __dredd_replace_unary_operator_PostDec_int(std::function<int&()> arg, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg()--;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg()++;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return ~arg();
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return -arg();
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return !arg();
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg();
  return arg()--;
}

)";
  const int kNumReplacementsOpt = 5;

  // Test with optimisations
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo() {
  int x = 2;
  __dredd_replace_unary_operator_PostDec_int([&]() -> int& { return static_cast<int&>(x); }, 0);
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static int __dredd_replace_unary_operator_PostDec_int(std::function<int&()> arg, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg()--;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg()++;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return ~arg();
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return -arg();
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return !arg();
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg();
  return arg()--;
}

)";
  const int kNumReplacementsNoOpt = 5;

  // Test without optimisations
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceUnaryOperatorTest, MutateDecrementAssign) {
  const std::string original = R"(void foo() {
  int x = 5;
  --x = 2;
}
)";
  const std::string expected_opt =
      R"(void foo() {
  int x = 5;
  __dredd_replace_unary_operator_PreDec_int([&]() -> int& { return static_cast<int&>(x); }, 0) = 2;
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static int& __dredd_replace_unary_operator_PreDec_int(std::function<int&()> arg, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return --arg();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return ++arg();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg();
  return --arg();
}

)";
  const int kNumReplacementsOpt = 2;

  // Test with optimisations
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo() {
  int x = 5;
  __dredd_replace_unary_operator_PreDec_int([&]() -> int& { return static_cast<int&>(x); }, 0) = 2;
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static int& __dredd_replace_unary_operator_PreDec_int(std::function<int&()> arg, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return --arg();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return ++arg();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg();
  return --arg();
}

)";
  const int kNumReplacementsNoOpt = 2;

  // Test without optimisations
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

}  // namespace
}  // namespace dredd
