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

#include "libdredd/mutation_remove_stmt.h"

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
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/RewriteBuffer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/Tooling.h"
#include "libdreddtest/gtest.h"
#include "llvm/ADT/SmallVector.h"

namespace dredd {
namespace {

void TestRemoval(const std::string& original, const std::string& expected,
                 std::function<MutationRemoveStmt(const clang::Preprocessor&,
                                                  clang::ASTContext&)>
                     mutation_supplier) {
  auto ast_unit = clang::tooling::buildASTFromCodeWithArgs(original, {"-w"});
  ASSERT_FALSE(ast_unit->getDiagnostics().hasErrorOccurred());
  clang::Rewriter rewriter(ast_unit->getSourceManager(),
                           ast_unit->getLangOpts());
  int mutation_id = 0;
  std::unordered_set<std::string> dredd_declarations;
  mutation_supplier(ast_unit->getPreprocessor(), ast_unit->getASTContext())
      .Apply(ast_unit->getASTContext(), ast_unit->getPreprocessor(), true,
             false, 0, mutation_id, rewriter, dredd_declarations);
  ASSERT_EQ(1, mutation_id);
  ASSERT_EQ(0, dredd_declarations.size());
  const clang::RewriteBuffer* rewrite_buffer = rewriter.getRewriteBufferFor(
      ast_unit->getSourceManager().getMainFileID());
  const std::string rewritten_text(rewrite_buffer->begin(),
                                   rewrite_buffer->end());
  ASSERT_EQ(expected, rewritten_text);
}

TEST(MutationRemoveStmtTest, BasicTest) {
  const std::string original = "void foo() { 1 + 2; }";
  const std::string expected =
      R"(void foo() { if (!__dredd_enabled_mutation(0)) { 1 + 2; } })";
  const std::function<MutationRemoveStmt(const clang::Preprocessor&,
                                         clang::ASTContext&)>
      mutation_supplier =
          [](const clang::Preprocessor& preprocessor,
             clang::ASTContext& ast_context) -> MutationRemoveStmt {
    auto statement = clang::ast_matchers::match(
        clang::ast_matchers::binaryOperator().bind("op"), ast_context);
    EXPECT_EQ(1, statement.size());
    return {*statement[0].getNodeAs<clang::BinaryOperator>("op"), preprocessor,
            ast_context};
  };
  TestRemoval(original, expected, mutation_supplier);
}

TEST(MutationRemoveStmtTest, RemoveIfStatement) {
  const std::string original = "void foo() { if (true) { } }";
  const std::string expected =
      R"(void foo() { if (!__dredd_enabled_mutation(0)) { if (true) { } } })";
  const std::function<MutationRemoveStmt(const clang::Preprocessor&,
                                         clang::ASTContext&)>
      mutation_supplier =
          [](const clang::Preprocessor& preprocessor,
             clang::ASTContext& ast_context) -> MutationRemoveStmt {
    auto statement = clang::ast_matchers::match(
        clang::ast_matchers::ifStmt().bind("if"), ast_context);
    EXPECT_EQ(1, statement.size());
    return {*statement[0].getNodeAs<clang::IfStmt>("if"), preprocessor,
            ast_context};
  };
  TestRemoval(original, expected, mutation_supplier);
}

TEST(MutationRemoveStmtTest, RemoveIfStatementWithTrailingSemi) {
  const std::string original = "void foo() { if (true) { }; }";
  const std::string expected =
      R"(void foo() { if (!__dredd_enabled_mutation(0)) { if (true) { }; } })";
  const std::function<MutationRemoveStmt(const clang::Preprocessor&,
                                         clang::ASTContext&)>
      mutation_supplier =
          [](const clang::Preprocessor& preprocessor,
             clang::ASTContext& ast_context) -> MutationRemoveStmt {
    auto statement = clang::ast_matchers::match(
        clang::ast_matchers::ifStmt().bind("if"), ast_context);
    EXPECT_EQ(1, statement.size());
    return {*statement[0].getNodeAs<clang::IfStmt>("if"), preprocessor,
            ast_context};
  };
  TestRemoval(original, expected, mutation_supplier);
}

TEST(MutationRemoveStmtTest, RemoveIfStatementWithTrailingSemis) {
  const std::string original = "void foo() { if (true) { };; }";
  const std::string expected =
      R"(void foo() { if (!__dredd_enabled_mutation(0)) { if (true) { }; }; })";
  const std::function<MutationRemoveStmt(const clang::Preprocessor&,
                                         clang::ASTContext&)>
      mutation_supplier =
          [](const clang::Preprocessor& preprocessor,
             clang::ASTContext& ast_context) -> MutationRemoveStmt {
    auto statement = clang::ast_matchers::match(
        clang::ast_matchers::ifStmt().bind("if"), ast_context);
    EXPECT_EQ(1, statement.size());
    return {*statement[0].getNodeAs<clang::IfStmt>("if"), preprocessor,
            ast_context};
  };
  TestRemoval(original, expected, mutation_supplier);
}

TEST(MutationRemoveStmtTest, RemoveIfStatementWithoutBraces) {
  const std::string original = "void foo() { if (true) return; }";
  const std::string expected =
      R"(void foo() { if (!__dredd_enabled_mutation(0)) { if (true) return; } })";
  const std::function<MutationRemoveStmt(const clang::Preprocessor&,
                                         clang::ASTContext&)>
      mutation_supplier =
          [](const clang::Preprocessor& preprocessor,
             clang::ASTContext& ast_context) -> MutationRemoveStmt {
    auto statement = clang::ast_matchers::match(
        clang::ast_matchers::ifStmt().bind("if"), ast_context);
    EXPECT_EQ(1, statement.size());
    return {*statement[0].getNodeAs<clang::IfStmt>("if"), preprocessor,
            ast_context};
  };
  TestRemoval(original, expected, mutation_supplier);
}

TEST(MutationRemoveStmtTest, RemoveReturnStmt) {
  const std::string original = "void foo() { return; }";
  const std::string expected =
      R"(void foo() { if (!__dredd_enabled_mutation(0)) { return; } })";
  const std::function<MutationRemoveStmt(const clang::Preprocessor&,
                                         clang::ASTContext&)>
      mutation_supplier =
          [](const clang::Preprocessor& preprocessor,
             clang::ASTContext& ast_context) -> MutationRemoveStmt {
    auto statement = clang::ast_matchers::match(
        clang::ast_matchers::returnStmt().bind("return"), ast_context);
    EXPECT_EQ(1, statement.size());
    return {*statement[0].getNodeAs<clang::ReturnStmt>("return"), preprocessor,
            ast_context};
  };
  TestRemoval(original, expected, mutation_supplier);
}

TEST(MutationRemoveStmtTest, RemoveBreakStmt) {
  const std::string original = "void foo() { while (true) { break; } }";
  const std::string expected =
      R"(void foo() { while (true) { if (!__dredd_enabled_mutation(0)) { break; } } })";
  const std::function<MutationRemoveStmt(const clang::Preprocessor&,
                                         clang::ASTContext&)>
      mutation_supplier =
          [](const clang::Preprocessor& preprocessor,
             clang::ASTContext& ast_context) -> MutationRemoveStmt {
    auto statement = clang::ast_matchers::match(
        clang::ast_matchers::breakStmt().bind("break"), ast_context);
    EXPECT_EQ(1, statement.size());
    return {*statement[0].getNodeAs<clang::BreakStmt>("break"), preprocessor,
            ast_context};
  };
  TestRemoval(original, expected, mutation_supplier);
}

TEST(MutationRemoveStmtTest, RemoveMacroStmt) {
  const std::string original =
      "#define ASSIGN(A, B) A = B\n"
      "void foo() { int x; ASSIGN(x, 1); }";
  const std::string expected =
      R"(#define ASSIGN(A, B) A = B
void foo() { int x; if (!__dredd_enabled_mutation(0)) { ASSIGN(x, 1); } })";
  const std::function<MutationRemoveStmt(const clang::Preprocessor&,
                                         clang::ASTContext&)>
      mutation_supplier =
          [](const clang::Preprocessor& preprocessor,
             clang::ASTContext& ast_context) -> MutationRemoveStmt {
    auto statement = clang::ast_matchers::match(
        clang::ast_matchers::binaryOperator().bind("assign"), ast_context);
    EXPECT_EQ(1, statement.size());
    return {*statement[0].getNodeAs<clang::BinaryOperator>("assign"),
            preprocessor, ast_context};
  };
  TestRemoval(original, expected, mutation_supplier);
}

}  // namespace
}  // namespace dredd
