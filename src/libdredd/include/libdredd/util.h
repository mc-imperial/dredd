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

#ifndef LIBDREDD_UTIL_H
#define LIBDREDD_UTIL_H

#include <string>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ParentMapContext.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Lex/Preprocessor.h"
#include "llvm/ADT/APFloat.h"

namespace dredd {

class InfoForSourceRange {
 public:
  InfoForSourceRange(clang::SourceRange source_range,
                     const clang::ASTContext& ast_context);
  [[nodiscard]] unsigned int GetStartLine() const { return start_line_; }

  [[nodiscard]] unsigned int GetStartColumn() const { return start_column_; }

  [[nodiscard]] unsigned int GetEndLine() const { return end_line_; }

  [[nodiscard]] unsigned int GetEndColumn() const { return end_column_; }

  [[nodiscard]] const std::string& GetSnippet() const { return snippet_; }

 private:
  unsigned int start_line_;
  unsigned int start_column_;
  unsigned int end_line_;
  unsigned int end_column_;
  std::string snippet_;
};

std::string SpaceToUnderscore(const std::string& input);

[[nodiscard]] clang::SourceRange GetSourceRangeInMainFile(
    const clang::Preprocessor& preprocessor,
    const clang::SourceRange& source_range);

template <typename HasSourceRange>
[[nodiscard]] clang::SourceRange GetSourceRangeInMainFile(
    const clang::Preprocessor& preprocessor, const HasSourceRange& ast_node) {
  return GetSourceRangeInMainFile(preprocessor, ast_node.getSourceRange());
}

// Introduced to work around what seems like a bug in Clang, where a source
// range can end earlier than it starts. See "structured_binding.cc" under
// single file tests.
bool SourceRangeConsistencyCheck(clang::SourceRange source_range,
                                 const clang::ASTContext& ast_context);

// Delegates to Expr::EvaluateAsBooleanCondition, but only if the expression is
// not value-dependent.
bool EvaluateAsBooleanCondition(const clang::Expr& expr,
                                const clang::ASTContext& ast_context,
                                bool& result);

// Delegates to Expr::EvaluateAsInt, but only if the expression is not
// value-dependent.
bool EvaluateAsInt(const clang::Expr& expr,
                   const clang::ASTContext& ast_context,
                   clang::Expr::EvalResult& result);

// Delegates to Expr::EvaluateAsFloat, but only if the expression is not
// value-dependent.
bool EvaluateAsFloat(const clang::Expr& expr,
                     const clang::ASTContext& ast_context,
                     llvm::APFloat& result);

// A wrapper for Expr::isCXX11ConstantExpr that first checks whether the
// expression is value-dependent (isCXX11ConstantExpr fails an assertion if the
// expression is value-dependent).
bool IsCxx11ConstantExpr(const clang::Expr& expr,
                         const clang::ASTContext& ast_context);

// It is often necessary to ask whether a given statement (which includes
// expressions) has a parent of a given type. This helper returns nullptr if
// the given statement has no parent of the template parameter type, and
// otherwise returns the first parent that does have the template parameter
// type.
template <typename RequiredParentT>
const RequiredParentT* GetFirstParentOfType(const clang::Stmt& stmt,
                                            clang::ASTContext& ast_context) {
  for (const auto& parent : ast_context.getParents(stmt)) {
    const auto* candidate_result = parent.template get<RequiredParentT>();
    if (candidate_result != nullptr) {
      return candidate_result;
    }
  }
  return nullptr;
}

}  // namespace dredd

#endif  // LIBDREDD_UTIL_H
