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

  auto binary_operator = clang::ast_matchers::match(
      clang::ast_matchers::binaryOperator().bind("op"),
      ast_unit->getASTContext());
  ASSERT_EQ(1, binary_operator.size());

  const MutationReplaceBinaryOperator mutation(
      *binary_operator[0].getNodeAs<clang::BinaryOperator>("op"),
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

TEST(MutationReplaceBinaryOperatorTest, MutateAdd) {
  const std::string original = "void foo() { 1 + 2; }";
  const std::string expected_opt =
      "void foo() { "
      "__dredd_replace_binary_operator_Add_arg1_int_arg2_int_lhs_one(1 , 2, "
      "0); "
      "}";
  const std::string expected_dredd_declaration_opt =
      R"(static int __dredd_replace_binary_operator_Add_arg1_int_arg2_int_lhs_one(int arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 + arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 / arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 % arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 - arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg2;
  return arg1 + arg2;
}

)";
  const int kNumReplacementsOpt = 4;
  // Test with optimisations enabled.
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      "void foo() { __dredd_replace_binary_operator_Add_arg1_int_arg2_int(1 , "
      "2, 0); "
      "}";
  const std::string expected_dredd_declaration_no_opt =
      R"(static int __dredd_replace_binary_operator_Add_arg1_int_arg2_int(int arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 + arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 / arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 * arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 % arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1 - arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg1;
  if (__dredd_enabled_mutation(local_mutation_id + 5)) return arg2;
  return arg1 + arg2;
}

)";
  const int kNumReplacementsNoOpt = 6;
  // Test without optimisations enabled.
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceBinaryOperatorTest, MutateLAnd) {
  const std::string original = R"(void foo(int x, int y) {
  bool z = x && y;
}
)";
  const std::string expected_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_LAnd_arg1_bool_arg2_bool(x , [&]() -> bool { return static_cast<bool>(y); }, 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static bool __dredd_replace_binary_operator_LAnd_arg1_bool_arg2_bool(bool arg1, std::function<bool()> arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 && arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 == arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg2();
  return arg1 && arg2();
}

)";
  const int kNumReplacementsOpt = 3;
  // Test with optimisations enabled.
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_LAnd_arg1_bool_arg2_bool(x , [&]() -> bool { return static_cast<bool>(y); }, 0);
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static bool __dredd_replace_binary_operator_LAnd_arg1_bool_arg2_bool(bool arg1, std::function<bool()> arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 && arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 || arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 == arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 != arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg2();
  return arg1 && arg2();
}

)";
  const int kNumReplacementsNoOpt = 5;
  // Test without optimisations enabled.
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceBinaryOperatorTest, MutateLOr) {
  const std::string original = R"(void foo(int x, int y) {
  bool z = x || y;
}
)";
  const std::string expected_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_LOr_arg1_bool_arg2_bool(x , [&]() -> bool { return static_cast<bool>(y); }, 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static bool __dredd_replace_binary_operator_LOr_arg1_bool_arg2_bool(bool arg1, std::function<bool()> arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 || arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 != arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg2();
  return arg1 || arg2();
}

)";
  const int kNumReplacementsOpt = 3;
  // Test with optimisations enabled.
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_LOr_arg1_bool_arg2_bool(x , [&]() -> bool { return static_cast<bool>(y); }, 0);
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static bool __dredd_replace_binary_operator_LOr_arg1_bool_arg2_bool(bool arg1, std::function<bool()> arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 || arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 && arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 == arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 != arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg2();
  return arg1 || arg2();
}

)";
  const int kNumReplacementsNoOpt = 5;
  // Test without optimisations enabled.
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceBinaryOperatorTest, MutateGT) {
  const std::string original = R"(void foo(int x, int y) {
  bool z = x > y;
}
)";
  const std::string expected_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_GT_arg1_int_arg2_int(x , y, 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static bool __dredd_replace_binary_operator_GT_arg1_int_arg2_int(int arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 > arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 != arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 >= arg2;
  return arg1 > arg2;
}

)";
  const int kNumReplacementsOpt = 2;
  // Test with optimisations enabled.
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_GT_arg1_int_arg2_int(x , y, 0);
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static bool __dredd_replace_binary_operator_GT_arg1_int_arg2_int(int arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 > arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 == arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 != arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 >= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1 <= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg1 < arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 5)) return arg1;
  if (__dredd_enabled_mutation(local_mutation_id + 6)) return arg2;
  return arg1 > arg2;
}

)";
  const int kNumReplacementsNoOpt = 7;
  // Test without optimisations enabled.
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceBinaryOperatorTest, MutateLT) {
  const std::string original = R"(void foo(int x, int y) {
  bool z = x < y;
}
)";
  const std::string expected_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_LT_arg1_int_arg2_int(x , y, 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static bool __dredd_replace_binary_operator_LT_arg1_int_arg2_int(int arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 < arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 != arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 <= arg2;
  return arg1 < arg2;
}

)";
  const int kNumReplacementsOpt = 2;
  // Test with optimisations enabled.
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_LT_arg1_int_arg2_int(x , y, 0);
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static bool __dredd_replace_binary_operator_LT_arg1_int_arg2_int(int arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 < arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 == arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 != arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 >= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1 > arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg1 <= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 5)) return arg1;
  if (__dredd_enabled_mutation(local_mutation_id + 6)) return arg2;
  return arg1 < arg2;
}

)";
  const int kNumReplacementsNoOpt = 7;
  // Test without optimisations enabled.
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceBinaryOperatorTest, MutateEQ) {
  const std::string original = R"(void foo(int x, int y) {
  bool z = x == y;
}
)";
  const std::string expected_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_EQ_arg1_int_arg2_int(x , y, 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static bool __dredd_replace_binary_operator_EQ_arg1_int_arg2_int(int arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 == arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 >= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 <= arg2;
  return arg1 == arg2;
}

)";
  const int kNumReplacementsOpt = 2;
  // Test with optimisations enabled.
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_EQ_arg1_int_arg2_int(x , y, 0);
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static bool __dredd_replace_binary_operator_EQ_arg1_int_arg2_int(int arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 == arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 != arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 >= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 > arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1 <= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg1 < arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 5)) return arg1;
  if (__dredd_enabled_mutation(local_mutation_id + 6)) return arg2;
  return arg1 == arg2;
}

)";
  const int kNumReplacementsNoOpt = 7;
  // Test without optimisations enabled.
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceBinaryOperatorTest, MutateGE) {
  const std::string original = R"(void foo(int x, int y) {
  bool z = x >= y;
}
)";
  const std::string expected_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_GE_arg1_int_arg2_int(x , y, 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static bool __dredd_replace_binary_operator_GE_arg1_int_arg2_int(int arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 >= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 == arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 > arg2;
  return arg1 >= arg2;
}

)";
  const int kNumReplacementsOpt = 2;
  // Test with optimisations enabled.
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_GE_arg1_int_arg2_int(x , y, 0);
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static bool __dredd_replace_binary_operator_GE_arg1_int_arg2_int(int arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 >= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 == arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 != arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 > arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1 <= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg1 < arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 5)) return arg1;
  if (__dredd_enabled_mutation(local_mutation_id + 6)) return arg2;
  return arg1 >= arg2;
}

)";
  const int kNumReplacementsNoOpt = 7;
  // Test without optimisations enabled.
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceBinaryOperatorTest, MutateLE) {
  const std::string original = R"(void foo(int x, int y) {
  bool z = x <= y;
}
)";
  const std::string expected_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_LE_arg1_int_arg2_int(x , y, 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static bool __dredd_replace_binary_operator_LE_arg1_int_arg2_int(int arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 <= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 == arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 < arg2;
  return arg1 <= arg2;
}

)";
  const int kNumReplacementsOpt = 2;
  // Test with optimisations enabled.
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_LE_arg1_int_arg2_int(x , y, 0);
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static bool __dredd_replace_binary_operator_LE_arg1_int_arg2_int(int arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 <= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 == arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 != arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 >= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1 > arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg1 < arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 5)) return arg1;
  if (__dredd_enabled_mutation(local_mutation_id + 6)) return arg2;
  return arg1 <= arg2;
}

)";
  const int kNumReplacementsNoOpt = 7;
  // Test without optimisations enabled.
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceBinaryOperatorTest, MutateNE) {
  const std::string original = R"(void foo(int x, int y) {
  bool z = x != y;
}
)";
  const std::string expected_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_NE_arg1_int_arg2_int(x , y, 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static bool __dredd_replace_binary_operator_NE_arg1_int_arg2_int(int arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 != arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 > arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 < arg2;
  return arg1 != arg2;
}

)";
  const int kNumReplacementsOpt = 2;
  // Test with optimisations enabled.
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_NE_arg1_int_arg2_int(x , y, 0);
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static bool __dredd_replace_binary_operator_NE_arg1_int_arg2_int(int arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 != arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 == arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 >= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 > arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1 <= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg1 < arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 5)) return arg1;
  if (__dredd_enabled_mutation(local_mutation_id + 6)) return arg2;
  return arg1 != arg2;
}

)";
  const int kNumReplacementsNoOpt = 7;
  // Test without optimisations enabled.
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceBinaryOperatorTest, MutateAssign) {
  const std::string original = R"(void foo() {
  int x;
  x = 1;
}
)";
  const std::string expected_opt =
      R"(void foo() {
  int x;
  __dredd_replace_binary_operator_Assign_arg1_int_arg2_int(x , 1, 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static int& __dredd_replace_binary_operator_Assign_arg1_int_arg2_int(int& arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 = arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 += arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 &= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 /= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1 *= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg1 |= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 5)) return arg1 %= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 6)) return arg1 <<= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 7)) return arg1 >>= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 8)) return arg1 -= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 9)) return arg1 ^= arg2;
  return arg1 = arg2;
}

)";
  const int kNumReplacementsOpt = 10;
  // Test with optimisations enabled.
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo() {
  int x;
  __dredd_replace_binary_operator_Assign_arg1_int_arg2_int(x , 1, 0);
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static int& __dredd_replace_binary_operator_Assign_arg1_int_arg2_int(int& arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 = arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 += arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 &= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 /= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1 *= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg1 |= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 5)) return arg1 %= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 6)) return arg1 <<= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 7)) return arg1 >>= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 8)) return arg1 -= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 9)) return arg1 ^= arg2;
  return arg1 = arg2;
}

)";
  const int kNumReplacementsNoOpt = 10;
  // Test without optimisations enabled.
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceBinaryOperatorTest, MutateAssignWithMacros) {
  const std::string original = R"(#define VAR x
#define BING(X, Y, Z) (X ? Y : Z)
void foo() {
  int x;
  VAR = BING(1, 2, 3);
}
)";
  const std::string expected_opt =
      R"(#define VAR x
#define BING(X, Y, Z) (X ? Y : Z)
void foo() {
  int x;
  __dredd_replace_binary_operator_Assign_arg1_int_arg2_int(VAR , BING(1, 2, 3), 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static int& __dredd_replace_binary_operator_Assign_arg1_int_arg2_int(int& arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 = arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 += arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 &= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 /= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1 *= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg1 |= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 5)) return arg1 %= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 6)) return arg1 <<= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 7)) return arg1 >>= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 8)) return arg1 -= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 9)) return arg1 ^= arg2;
  return arg1 = arg2;
}

)";
  const int kNumReplacementsOpt = 10;
  // Test with optimisations enabled.
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(#define VAR x
#define BING(X, Y, Z) (X ? Y : Z)
void foo() {
  int x;
  __dredd_replace_binary_operator_Assign_arg1_int_arg2_int(VAR , BING(1, 2, 3), 0);
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static int& __dredd_replace_binary_operator_Assign_arg1_int_arg2_int(int& arg1, int arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 = arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 += arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 &= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 /= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1 *= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg1 |= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 5)) return arg1 %= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 6)) return arg1 <<= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 7)) return arg1 >>= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 8)) return arg1 -= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 9)) return arg1 ^= arg2;
  return arg1 = arg2;
}

)";
  const int kNumReplacementsNoOpt = 10;
  // Test with optimisations enabled.
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceBinaryOperatorTest, MutateFloatDiv) {
  const std::string original = R"(void foo() {
  float x = 6.43622;
  float y = 3.53462;
  float z = x / y;
}
)";
  const std::string expected_opt =
      R"(void foo() {
  float x = 6.43622;
  float y = 3.53462;
  float z = __dredd_replace_binary_operator_Div_arg1_float_arg2_float(x , y, 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static float __dredd_replace_binary_operator_Div_arg1_float_arg2_float(float arg1, float arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 / arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 + arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 * arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 - arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg2;
  return arg1 / arg2;
}

)";
  const int kNumReplacementsOpt = 5;
  // Test with optimisations enabled.
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo() {
  float x = 6.43622;
  float y = 3.53462;
  float z = __dredd_replace_binary_operator_Div_arg1_float_arg2_float(x , y, 0);
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static float __dredd_replace_binary_operator_Div_arg1_float_arg2_float(float arg1, float arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 / arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 + arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 * arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 - arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg2;
  return arg1 / arg2;
}

)";
  const int kNumReplacementsNoOpt = 5;
  // Test without optimisations enabled.
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceBinaryOperatorTest, MutateFloatSubAssign) {
  const std::string original = R"(void foo() {
  double x = 234.23532;
  double y = 0.65433;
  x -= y;
}
)";
  const std::string expected_opt =
      R"(void foo() {
  double x = 234.23532;
  double y = 0.65433;
  __dredd_replace_binary_operator_SubAssign_arg1_double_arg2_double(x , y, 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static double& __dredd_replace_binary_operator_SubAssign_arg1_double_arg2_double(double& arg1, double arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 -= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 += arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 = arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 /= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1 *= arg2;
  return arg1 -= arg2;
}

)";
  const int kNumReplacementsOpt = 4;
  // Test with optimisations enabled.
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo() {
  double x = 234.23532;
  double y = 0.65433;
  __dredd_replace_binary_operator_SubAssign_arg1_double_arg2_double(x , y, 0);
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static double& __dredd_replace_binary_operator_SubAssign_arg1_double_arg2_double(double& arg1, double arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 -= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 += arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 = arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 /= arg2;
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1 *= arg2;
  return arg1 -= arg2;
}

)";
  const int kNumReplacementsNoOpt = 4;
  // Test without optimisations enabled.
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceBinaryOperatorTest, MutateLAndWithLhsSideEffect) {
  const std::string original = R"(void foo(int x, int y) {
  bool z = (x++) && y;
}
)";
  const std::string expected_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_LAnd_arg1_bool_arg2_bool([&]() -> bool { return static_cast<bool>((x++)); } , [&]() -> bool { return static_cast<bool>(y); }, 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static bool __dredd_replace_binary_operator_LAnd_arg1_bool_arg2_bool(std::function<bool()> arg1, std::function<bool()> arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1() && arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1() == arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1();
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg2();
  return arg1() && arg2();
}

)";
  const int kNumReplacementsOpt = 3;
  // Test with optimisations enabled.
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_LAnd_arg1_bool_arg2_bool([&]() -> bool { return static_cast<bool>((x++)); } , [&]() -> bool { return static_cast<bool>(y); }, 0);
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static bool __dredd_replace_binary_operator_LAnd_arg1_bool_arg2_bool(std::function<bool()> arg1, std::function<bool()> arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1() && arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1() || arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1() == arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1() != arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1();
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg2();
  return arg1() && arg2();
}

)";
  const int kNumReplacementsNoOpt = 5;
  // Test without optimisations enabled.
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceBinaryOperatorTest, MutateLAndWithRhsSideEffect) {
  const std::string original = R"(void foo(int x, int y) {
  bool z = x && (y++);
}
)";
  const std::string expected_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_LAnd_arg1_bool_arg2_bool(x , [&]() -> bool { return static_cast<bool>((y++)); }, 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static bool __dredd_replace_binary_operator_LAnd_arg1_bool_arg2_bool(bool arg1, std::function<bool()> arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 && arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 == arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1;
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg2();
  return arg1 && arg2();
}

)";
  const int kNumReplacementsOpt = 3;
  // Test with optimisations enabled.
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_LAnd_arg1_bool_arg2_bool(x , [&]() -> bool { return static_cast<bool>((y++)); }, 0);
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static bool __dredd_replace_binary_operator_LAnd_arg1_bool_arg2_bool(bool arg1, std::function<bool()> arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1 && arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1 || arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1 == arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1 != arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1;
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg2();
  return arg1 && arg2();
}

)";
  const int kNumReplacementsNoOpt = 5;
  // Test without optimisations enabled.
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

TEST(MutationReplaceBinaryOperatorTest, MutateLAndWithLhsRhsSideEffect) {
  const std::string original = R"(void foo(int x, int y) {
  bool z = (x++) && (y++);
}
)";
  const std::string expected_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_LAnd_arg1_bool_arg2_bool([&]() -> bool { return static_cast<bool>((x++)); } , [&]() -> bool { return static_cast<bool>((y++)); }, 0);
}
)";
  const std::string expected_dredd_declaration_opt =
      R"(static bool __dredd_replace_binary_operator_LAnd_arg1_bool_arg2_bool(std::function<bool()> arg1, std::function<bool()> arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1() && arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1() == arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1();
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg2();
  return arg1() && arg2();
}

)";
  const int kNumReplacementsOpt = 3;
  // Test with optimisations enabled.
  TestReplacement(original, expected_opt, kNumReplacementsOpt, true,
                  expected_dredd_declaration_opt);

  const std::string expected_no_opt =
      R"(void foo(int x, int y) {
  bool z = __dredd_replace_binary_operator_LAnd_arg1_bool_arg2_bool([&]() -> bool { return static_cast<bool>((x++)); } , [&]() -> bool { return static_cast<bool>((y++)); }, 0);
}
)";
  const std::string expected_dredd_declaration_no_opt =
      R"(static bool __dredd_replace_binary_operator_LAnd_arg1_bool_arg2_bool(std::function<bool()> arg1, std::function<bool()> arg2, int local_mutation_id) {
  if (!__dredd_some_mutation_enabled) return arg1() && arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 0)) return arg1() || arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 1)) return arg1() == arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 2)) return arg1() != arg2();
  if (__dredd_enabled_mutation(local_mutation_id + 3)) return arg1();
  if (__dredd_enabled_mutation(local_mutation_id + 4)) return arg2();
  return arg1() && arg2();
}

)";
  const int kNumReplacementsNoOpt = 5;
  // Test without optimisations enabled.
  TestReplacement(original, expected_no_opt, kNumReplacementsNoOpt, false,
                  expected_dredd_declaration_no_opt);
}

}  // namespace
}  // namespace dredd
