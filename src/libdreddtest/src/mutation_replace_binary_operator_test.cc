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

#include <memory>
#include <string>

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
                     const std::string& expected_dredd_declaration) {
  auto ast_unit = clang::tooling::buildASTFromCodeWithArgs(original, {"-w"});
  ASSERT_FALSE(ast_unit->getDiagnostics().hasErrorOccurred());
  auto function_decl = clang::ast_matchers::match(
      clang::ast_matchers::functionDecl(clang::ast_matchers::hasName("foo"))
          .bind("fn"),
      ast_unit->getASTContext());
  ASSERT_EQ(1, function_decl.size());

  auto binary_operator = clang::ast_matchers::match(
      clang::ast_matchers::binaryOperator().bind("op"),
      ast_unit->getASTContext());
  ASSERT_EQ(1, binary_operator.size());

  MutationReplaceBinaryOperator mutation(
      *binary_operator[0].getNodeAs<clang::BinaryOperator>("op"));

  clang::Rewriter rewriter(ast_unit->getSourceManager(),
                           ast_unit->getLangOpts());
  int mutation_id = 0;
  std::unordered_set<std::string> dredd_declarations;
  mutation.Apply(ast_unit->getASTContext(), ast_unit->getPreprocessor(), 0,
                 mutation_id, rewriter, dredd_declarations);
  ASSERT_EQ(num_replacements, mutation_id);
  ASSERT_EQ(1, dredd_declarations.size());
  ASSERT_EQ(expected_dredd_declaration, *dredd_declarations.begin());

  const clang::RewriteBuffer* rewrite_buffer = rewriter.getRewriteBufferFor(
      ast_unit->getSourceManager().getMainFileID());
  std::string rewritten_text(rewrite_buffer->begin(), rewrite_buffer->end());
  ASSERT_EQ(expected, rewritten_text);
}

TEST(MutationReplaceBinaryOperatorTest, MutateAdd) {
  std::string original = "void foo() { 1 + 2; }";
  std::string expected =
      "void foo() { __dredd_replace_binary_operator_Add_int_int([&]() -> int { "
      "return "
      "static_cast<int>(1); }, [&]() -> int { return static_cast<int>(2); }, "
      "0); "
      "}";
  std::string expected_dredd_declaration =
      R"(static int __dredd_replace_binary_operator_Add_int_int(std::function<int()> arg1, std::function<int()> arg2, int local_mutation_id) {
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1() / arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1() * arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1() % arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1() - arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg1();
  if (__dredd_enabled_mutation(local_mutation_id + 5)) return arg2();
  return arg1() + arg2();
}

)";
  const int kNumReplacements = 6;
  TestReplacement(original, expected, kNumReplacements,
                  expected_dredd_declaration);
}

TEST(MutationReplaceBinaryOperatorTest, MutateAnd) {
  std::string original = R"(void foo() {
  int x = 1;
  int y = 2;
  int z = x && y;
}
)";
  std::string expected =
      R"(void foo() {
  int x = 1;
  int y = 2;
  int z = __dredd_replace_binary_operator_LAnd_bool_bool([&]() -> bool { return static_cast<bool>(x); }, [&]() -> bool { return static_cast<bool>(y); }, 0);
}
)";
  std::string expected_dredd_declaration =
      R"(static bool __dredd_replace_binary_operator_LAnd_bool_bool(std::function<bool()> arg1, std::function<bool()> arg2, int local_mutation_id) {
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1() || arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1();
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return true;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return false;
  return arg1() && arg2();
}

)";
  const int kNumReplacements = 5;
  TestReplacement(original, expected, kNumReplacements,
                  expected_dredd_declaration);
}

TEST(MutationReplaceBinaryOperatorTest, MutateAssign) {
  std::string original = R"(void foo() {
  int x;
  x = 1;
}
)";
  std::string expected =
      R"(void foo() {
  int x;
  __dredd_replace_binary_operator_Assign_int_int([&]() -> int& { return static_cast<int&>(x); }, [&]() -> int { return static_cast<int>(1); }, 0);
}
)";
  std::string expected_dredd_declaration =
      R"(static int& __dredd_replace_binary_operator_Assign_int_int(std::function<int&()> arg1, std::function<int()> arg2, int local_mutation_id) {
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1() += arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1() &= arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1() /= arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1() *= arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg1() |= arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 5)) return arg1() %= arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 6)) return arg1() <<= arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 7)) return arg1() >>= arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 8)) return arg1() -= arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 9)) return arg1() ^= arg2();
  return arg1() = arg2();
}

)";
  const int kNumReplacements = 10;
  TestReplacement(original, expected, kNumReplacements,
                  expected_dredd_declaration);
}

TEST(MutationReplaceBinaryOperatorTest, MutateAssignWithMacros) {
  std::string original = R"(#define VAR x
#define BING(X, Y, Z) (X ? Y : Z)
void foo() {
  int x;
  VAR = BING(1, 2, 3);
}
)";
  std::string expected =
      R"(#define VAR x
#define BING(X, Y, Z) (X ? Y : Z)
void foo() {
  int x;
  __dredd_replace_binary_operator_Assign_int_int([&]() -> int& { return static_cast<int&>(VAR); }, [&]() -> int { return static_cast<int>(BING(1, 2, 3)); }, 0);
}
)";
  std::string expected_dredd_declaration =
      R"(static int& __dredd_replace_binary_operator_Assign_int_int(std::function<int&()> arg1, std::function<int()> arg2, int local_mutation_id) {
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1() += arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1() &= arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1() /= arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1() *= arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg1() |= arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 5)) return arg1() %= arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 6)) return arg1() <<= arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 7)) return arg1() >>= arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 8)) return arg1() -= arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 9)) return arg1() ^= arg2();
  return arg1() = arg2();
}

)";
  const int kNumReplacements = 10;
  TestReplacement(original, expected, kNumReplacements,
                  expected_dredd_declaration);
}

TEST(MutationReplaceBinaryOperatorTest, MutateFloatDiv) {
  std::string original = R"(void foo() {
  float x = 6.43622;
  float y = 3.53462;
  float z = x / y;
}
)";
  std::string expected =
      R"(void foo() {
  float x = 6.43622;
  float y = 3.53462;
  float z = __dredd_replace_binary_operator_Div_float_float([&]() -> float { return static_cast<float>(x); }, [&]() -> float { return static_cast<float>(y); }, 0);
}
)";
  std::string expected_dredd_declaration =
      R"(static float __dredd_replace_binary_operator_Div_float_float(std::function<float()> arg1, std::function<float()> arg2, int local_mutation_id) {
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1() + arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1() * arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1() - arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1();
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg2();
  return arg1() / arg2();
}

)";
  const int kNumReplacements = 5;
  TestReplacement(original, expected, kNumReplacements,
                  expected_dredd_declaration);
}

TEST(MutationReplaceBinaryOperatorTest, MutateFloatSubAssign) {
  std::string original = R"(void foo() {
  double x = 234.23532;
  double y = 0.65433;
  x -= y;
}
)";
  std::string expected =
      R"(void foo() {
  double x = 234.23532;
  double y = 0.65433;
  __dredd_replace_binary_operator_SubAssign_double_double([&]() -> double& { return static_cast<double&>(x); }, [&]() -> double { return static_cast<double>(y); }, 0);
}
)";
  std::string expected_dredd_declaration =
      R"(static double& __dredd_replace_binary_operator_SubAssign_double_double(std::function<double&()> arg1, std::function<double()> arg2, int local_mutation_id) {
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1() += arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1() = arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1() /= arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1() *= arg2();
  return arg1() -= arg2();
}

)";
  const int kNumReplacements = 4;
  TestReplacement(original, expected, kNumReplacements,
                  expected_dredd_declaration);
}

}  // namespace
}  // namespace dredd
