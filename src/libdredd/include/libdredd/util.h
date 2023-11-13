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
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
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

template <typename HasSourceRange>
[[nodiscard]] clang::SourceRange GetSourceRangeInMainFile(
    const clang::Preprocessor& preprocessor, const HasSourceRange& ast_node) {
  const clang::SourceManager& source_manager = preprocessor.getSourceManager();
  auto main_file_id = source_manager.getMainFileID();

  clang::SourceLocation begin_loc_in_main_file;
  clang::SourceLocation end_loc_in_main_file;
  clang::SourceLocation macro_expansion_location;
  {
    const clang::SourceLocation begin_loc =
        ast_node.getSourceRange().getBegin();
    auto begin_file_id = source_manager.getFileID(begin_loc);
    if (begin_file_id == main_file_id) {
      begin_loc_in_main_file = begin_loc;
    } else if (begin_loc.isMacroID() &&
               preprocessor.isAtStartOfMacroExpansion(
                   begin_loc, &macro_expansion_location) &&
               source_manager.getFileID(macro_expansion_location) ==
                   main_file_id) {
      begin_loc_in_main_file = macro_expansion_location;
    } else {
      // There is no location in the main file corresponding to the start of the
      // AST node.
      return {};
    }
  }
  {
    const clang::SourceLocation end_loc = ast_node.getSourceRange().getEnd();
    auto end_file_id = source_manager.getFileID(end_loc);
    if (end_file_id == main_file_id) {
      end_loc_in_main_file = end_loc;
    } else if (end_loc.isMacroID() &&
               preprocessor.isAtEndOfMacroExpansion(
                   end_loc, &macro_expansion_location) &&
               source_manager.getFileID(macro_expansion_location) ==
                   main_file_id) {
      end_loc_in_main_file = macro_expansion_location;
    } else {
      // There is no location in the main file corresponding to the end of the
      // AST node.
      return {};
    }
  }

  return {begin_loc_in_main_file, end_loc_in_main_file};
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

}  // namespace dredd

#endif  // LIBDREDD_UTIL_H
