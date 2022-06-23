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
#include "clang/AST/Expr.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/PrettyPrinter.h"
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

TEST(MutationReplaceBinaryOperatorTest, BasicTest) {
  std::string original = "void foo() { 1 + 2; }";
  std::string expected =
      R"(static int __dredd_replace_binary_operator_0(int arg1, int arg2) {
  if (__dredd_enabled_mutation() == 0) {
    return arg1 - arg2;
  }
  return arg1 + arg2;
}

void foo() { __dredd_replace_binary_operator_0(1, 2); })";
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
      *binary_operator[0].getNodeAs<clang::BinaryOperator>("op"),
      *function_decl[0].getNodeAs<clang::FunctionDecl>("fn"),
      clang::BinaryOperatorKind::BO_Sub);

  clang::Rewriter rewriter(ast_unit->getSourceManager(),
                           ast_unit->getLangOpts());
  clang::PrintingPolicy printing_policy(ast_unit->getLangOpts());
  mutation.Apply(0, rewriter, printing_policy);

  const clang::RewriteBuffer* rewrite_buffer = rewriter.getRewriteBufferFor(
      ast_unit->getSourceManager().getMainFileID());
  std::string rewritten_text(rewrite_buffer->begin(), rewrite_buffer->end());
  ASSERT_EQ(expected, rewritten_text);
}

TEST(MutationReplaceBinaryOperatorTest, AndToShift) {
  std::string original = R"(void foo() {
  int x = 1;
  int y = 2;
  int z = x && y;
}
)";
  std::string expected =
      R"(static bool __dredd_replace_binary_operator_0(std::function<bool()> arg1, std::function<bool()> arg2) {
  if (__dredd_enabled_mutation() == 0) {
    return arg1() >> arg2();
  }
  return arg1() && arg2();
}

void foo() {
  int x = 1;
  int y = 2;
  int z = __dredd_replace_binary_operator_0([&]() -> bool { return static_cast<bool>(x); }, [&]() -> bool { return static_cast<bool>(y); });
}
)";
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
      *binary_operator[0].getNodeAs<clang::BinaryOperator>("op"),
      *function_decl[0].getNodeAs<clang::FunctionDecl>("fn"),
      clang::BinaryOperatorKind::BO_Shr);

  clang::Rewriter rewriter(ast_unit->getSourceManager(),
                           ast_unit->getLangOpts());
  clang::PrintingPolicy printing_policy(ast_unit->getLangOpts());
  mutation.Apply(0, rewriter, printing_policy);

  const clang::RewriteBuffer* rewrite_buffer = rewriter.getRewriteBufferFor(
      ast_unit->getSourceManager().getMainFileID());
  std::string rewritten_text(rewrite_buffer->begin(), rewrite_buffer->end());
  ASSERT_EQ(expected, rewritten_text);
}

TEST(MutationReplaceBinaryOperatorTest, AssignToAddAssign) {
  std::string original = R"(void foo() {
  int x;
  x = 1;
}
)";
  std::string expected =
      R"(static int& __dredd_replace_binary_operator_0(int& arg1, int arg2) {
  if (__dredd_enabled_mutation() == 0) {
    return arg1 += arg2;
  }
  return arg1 = arg2;
}

void foo() {
  int x;
  __dredd_replace_binary_operator_0(x, 1);
}
)";
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
      *binary_operator[0].getNodeAs<clang::BinaryOperator>("op"),
      *function_decl[0].getNodeAs<clang::FunctionDecl>("fn"),
      clang::BinaryOperatorKind::BO_AddAssign);

  clang::Rewriter rewriter(ast_unit->getSourceManager(),
                           ast_unit->getLangOpts());
  clang::PrintingPolicy printing_policy(ast_unit->getLangOpts());
  mutation.Apply(0, rewriter, printing_policy);

  const clang::RewriteBuffer* rewrite_buffer = rewriter.getRewriteBufferFor(
      ast_unit->getSourceManager().getMainFileID());
  std::string rewritten_text(rewrite_buffer->begin(), rewrite_buffer->end());
  ASSERT_EQ(expected, rewritten_text);
}

}  // namespace
}  // namespace dredd
