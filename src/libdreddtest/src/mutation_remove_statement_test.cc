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

#include "libdredd/mutation_remove_statement.h"

#include <functional>
#include <memory>
#include <string>

#include "clang/AST/ASTContext.h"
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

void TestRemoval(const std::string& original, const std::string& expected,
                 std::function<MutationRemoveStatement(clang::ASTContext&)>
                     mutation_supplier) {
  auto ast_unit = clang::tooling::buildASTFromCodeWithArgs(original, {"-w"});
  ASSERT_FALSE(ast_unit->getDiagnostics().hasErrorOccurred());
  clang::Rewriter rewriter(ast_unit->getSourceManager(),
                           ast_unit->getLangOpts());
  mutation_supplier(ast_unit->getASTContext())
      .Apply(0, ast_unit->getASTContext(), rewriter);
  const clang::RewriteBuffer* rewrite_buffer = rewriter.getRewriteBufferFor(
      ast_unit->getSourceManager().getMainFileID());
  std::string rewritten_text(rewrite_buffer->begin(), rewrite_buffer->end());
  ASSERT_EQ(expected, rewritten_text);
}

TEST(MutationRemoveStatementTest, BasicTest) {
  std::string original = "void foo() { 1 + 2; }";
  std::string expected =
      R"(void foo() { if (__dredd_enabled_mutation() != 0) { 1 + 2; } })";
  std::function<MutationRemoveStatement(clang::ASTContext&)> mutation_supplier =
      [](clang::ASTContext& ast_context) -> MutationRemoveStatement {
    auto statement = clang::ast_matchers::match(
        clang::ast_matchers::binaryOperator().bind("op"), ast_context);
    EXPECT_EQ(1, statement.size());
    return MutationRemoveStatement(
        *statement[0].getNodeAs<clang::BinaryOperator>("op"));
  };
  TestRemoval(original, expected, mutation_supplier);
}

TEST(MutationRemoveStatementTest, RemoveIfStatement) {
  std::string original = "void foo() { if (true) { } }";
  std::string expected =
      R"(void foo() { if (__dredd_enabled_mutation() != 0) { if (true) { } } })";
  std::function<MutationRemoveStatement(clang::ASTContext&)> mutation_supplier =
      [](clang::ASTContext& ast_context) -> MutationRemoveStatement {
    auto statement = clang::ast_matchers::match(
        clang::ast_matchers::ifStmt().bind("if"), ast_context);
    EXPECT_EQ(1, statement.size());
    return MutationRemoveStatement(
        *statement[0].getNodeAs<clang::IfStmt>("if"));
  };
  TestRemoval(original, expected, mutation_supplier);
}

TEST(MutationRemoveStatementTest, RemoveIfStatementWithTrailingSemi) {
  std::string original = "void foo() { if (true) { }; }";
  std::string expected =
      R"(void foo() { if (__dredd_enabled_mutation() != 0) { if (true) { }; } })";
  std::function<MutationRemoveStatement(clang::ASTContext&)> mutation_supplier =
      [](clang::ASTContext& ast_context) -> MutationRemoveStatement {
    auto statement = clang::ast_matchers::match(
        clang::ast_matchers::ifStmt().bind("if"), ast_context);
    EXPECT_EQ(1, statement.size());
    return MutationRemoveStatement(
        *statement[0].getNodeAs<clang::IfStmt>("if"));
  };
  TestRemoval(original, expected, mutation_supplier);
}

TEST(MutationRemoveStatementTest, RemoveIfStatementWithTrailingSemis) {
  std::string original = "void foo() { if (true) { };; }";
  std::string expected =
      R"(void foo() { if (__dredd_enabled_mutation() != 0) { if (true) { }; }; })";
  std::function<MutationRemoveStatement(clang::ASTContext&)> mutation_supplier =
      [](clang::ASTContext& ast_context) -> MutationRemoveStatement {
    auto statement = clang::ast_matchers::match(
        clang::ast_matchers::ifStmt().bind("if"), ast_context);
    EXPECT_EQ(1, statement.size());
    return MutationRemoveStatement(
        *statement[0].getNodeAs<clang::IfStmt>("if"));
  };
  TestRemoval(original, expected, mutation_supplier);
}

TEST(MutationRemoveStatementTest, RemoveIfStatementWithoutBraces) {
  std::string original = "void foo() { if (true) return; }";
  std::string expected =
      R"(void foo() { if (__dredd_enabled_mutation() != 0) { if (true) return; } })";
  std::function<MutationRemoveStatement(clang::ASTContext&)> mutation_supplier =
      [](clang::ASTContext& ast_context) -> MutationRemoveStatement {
    auto statement = clang::ast_matchers::match(
        clang::ast_matchers::ifStmt().bind("if"), ast_context);
    EXPECT_EQ(1, statement.size());
    return MutationRemoveStatement(
        *statement[0].getNodeAs<clang::IfStmt>("if"));
  };
  TestRemoval(original, expected, mutation_supplier);
}

TEST(MutationRemoveStatementTest, RemoveReturnStmt) {
  std::string original = "void foo() { return; }";
  std::string expected =
      R"(void foo() { if (__dredd_enabled_mutation() != 0) { return; } })";
  std::function<MutationRemoveStatement(clang::ASTContext&)> mutation_supplier =
      [](clang::ASTContext& ast_context) -> MutationRemoveStatement {
    auto statement = clang::ast_matchers::match(
        clang::ast_matchers::returnStmt().bind("return"), ast_context);
    EXPECT_EQ(1, statement.size());
    return MutationRemoveStatement(
        *statement[0].getNodeAs<clang::ReturnStmt>("return"));
  };
  TestRemoval(original, expected, mutation_supplier);
}

TEST(MutationRemoveStatementTest, RemoveBreakStmt) {
  std::string original = "void foo() { while (true) { break; } }";
  std::string expected =
      R"(void foo() { while (true) { if (__dredd_enabled_mutation() != 0) { break; } } })";
  std::function<MutationRemoveStatement(clang::ASTContext&)> mutation_supplier =
      [](clang::ASTContext& ast_context) -> MutationRemoveStatement {
    auto statement = clang::ast_matchers::match(
        clang::ast_matchers::breakStmt().bind("break"), ast_context);
    EXPECT_EQ(1, statement.size());
    return MutationRemoveStatement(
        *statement[0].getNodeAs<clang::BreakStmt>("break"));
  };
  TestRemoval(original, expected, mutation_supplier);
}

}  // namespace
}  // namespace dredd
