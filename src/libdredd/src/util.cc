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
// limitations under the License

#include "libdredd/util.h"

#include <algorithm>
#include <cassert>
#include <utility>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/Preprocessor.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/StringRef.h"

namespace dredd {

namespace {

clang::SourceLocation getSourceLocationInMainFileForExtremeOfRange(
    const clang::SourceLocation& extreme_of_range,
    const clang::Preprocessor& preprocessor, bool is_start_of_range) {
  const clang::SourceManager& source_manager = preprocessor.getSourceManager();
  auto file_id = source_manager.getFileID(extreme_of_range);
  if (file_id == source_manager.getMainFileID()) {
    // This is the easy case: the source location occurs directly in the main
    // file.
    return extreme_of_range;
  }
  // The source location's file ID is not the main file's ID. This means it is
  // either part of a macro expansion, or it is in some other file.
  if (!extreme_of_range.isMacroID()) {
    // The source location is in some other file.
    return {};
  }
  // The source location is part of a macro expansion. If it is part of a
  // "paste" operation in a macro, which uses the ## preprocessor operator, then
  // it is hard to trace this back accurately to a non-macro source location,
  // so we give up. Testing for this is done by checking whether source location
  // is written into "scratch" space, which is the case for pasted macro
  // expansions.
  if (preprocessor.getSourceManager().isWrittenInScratchSpace(
          source_manager.getSpellingLoc(extreme_of_range))) {
    return {};
  }
  clang::SourceLocation macro_expansion_location;
  // We now check whether the macro expansion location is at the start/end of
  // the expansion, depending on whether the source location corresponds to the
  // start/end of a range.
  if (is_start_of_range) {
    if (!preprocessor.isAtStartOfMacroExpansion(extreme_of_range,
                                                &macro_expansion_location)) {
      // The source location is somewhere in the middle of a macro expansion -
      // it cannot be traced back to the main file.
      return {};
    }
  } else {
    if (!preprocessor.isAtEndOfMacroExpansion(extreme_of_range,
                                              &macro_expansion_location)) {
      // Similar.
      return {};
    }
  }
  // The source location does correspond to the extreme of a macro expansion.
  // Finally, we check whether the expansion location is in the main file or
  // some other file.
  if (source_manager.getFileID(macro_expansion_location) ==
      source_manager.getMainFileID()) {
    // The expansion location is in the main file, so can be returned.
    return macro_expansion_location;
  }
  // Tne expansion location is in some file other than the main file.
  return {};
}
}  // namespace

// Utility method used to avoid spaces when types, such as 'unsigned int', are
// used in mutation function names.
std::string SpaceToUnderscore(const std::string& input) {
  std::string result(input);
  std::replace(result.begin(), result.end(), ' ', '_');
  return result;
}

bool SourceRangeConsistencyCheck(clang::SourceRange source_range,
                                 const clang::ASTContext& ast_context) {
  const auto& source_manager = ast_context.getSourceManager();
  auto char_source_range = clang::CharSourceRange::getTokenRange(source_range);
  assert(char_source_range.isTokenRange() && "Expected a token range.");
  (void)char_source_range;  // Keep release-mode compilers happy.
  const unsigned int final_token_length = clang::Lexer::MeasureTokenLength(
      source_range.getEnd(), source_manager, ast_context.getLangOpts());

  const unsigned int start_line =
      source_manager.getSpellingLineNumber(source_range.getBegin());
  const unsigned int start_column =
      source_manager.getSpellingColumnNumber(source_range.getBegin());
  const unsigned int end_line =
      source_manager.getSpellingLineNumber(source_range.getEnd());
  const unsigned int end_column =
      source_manager.getSpellingColumnNumber(source_range.getEnd()) +
      final_token_length;

  return start_line < end_line || start_column < end_column;
}

InfoForSourceRange::InfoForSourceRange(clang::SourceRange source_range,
                                       const clang::ASTContext& ast_context) {
  const auto& source_manager = ast_context.getSourceManager();
  auto char_source_range = clang::CharSourceRange::getTokenRange(source_range);
  assert(char_source_range.isTokenRange() && "Expected a token range.");
  (void)char_source_range;  // Keep release-mode compilers happy.
  const unsigned int final_token_length = clang::Lexer::MeasureTokenLength(
      source_range.getEnd(), source_manager, ast_context.getLangOpts());

  auto start_loc_decomposed =
      source_manager.getDecomposedLoc(source_range.getBegin());
  auto end_loc_decomposed =
      source_manager.getDecomposedLoc(source_range.getEnd());
  auto buffer_data = source_manager.getBufferData(start_loc_decomposed.first);

  start_line_ = source_manager.getSpellingLineNumber(source_range.getBegin());
  start_column_ =
      source_manager.getSpellingColumnNumber(source_range.getBegin());
  end_line_ = source_manager.getSpellingLineNumber(source_range.getEnd());
  end_column_ = source_manager.getSpellingColumnNumber(source_range.getEnd()) +
                final_token_length;

  assert((start_line_ < end_line_ || start_column_ < end_column_) &&
         "Bad source range.");

  const unsigned int length = end_loc_decomposed.second -
                              start_loc_decomposed.second + final_token_length;

  const std::string kSnipText(" ... [snip] ... ");
  const unsigned int kSnippetLengthEachSide = 10;
  const unsigned int kMinSnippedLength =
      static_cast<unsigned int>(kSnipText.size()) + 2 * kSnippetLengthEachSide;
  if (length <= kMinSnippedLength) {
    snippet_ = buffer_data.substr(start_loc_decomposed.second, length).str();
  } else {
    snippet_ =
        buffer_data.substr(start_loc_decomposed.second, kSnippetLengthEachSide)
            .str() +
        kSnipText +
        buffer_data
            .substr(
                start_loc_decomposed.second + length - kSnippetLengthEachSide,
                kSnippetLengthEachSide)
            .str();
  }
}

bool EvaluateAsBooleanCondition(const clang::Expr& expr,
                                const clang::ASTContext& ast_context,
                                bool& result) {
  return !expr.isValueDependent() &&
         expr.EvaluateAsBooleanCondition(result, ast_context);
}

bool EvaluateAsInt(const clang::Expr& expr,
                   const clang::ASTContext& ast_context,
                   clang::Expr::EvalResult& result) {
  return !expr.isValueDependent() && expr.EvaluateAsInt(result, ast_context);
}

bool EvaluateAsFloat(const clang::Expr& expr,
                     const clang::ASTContext& ast_context,
                     llvm::APFloat& result) {
  return !expr.isValueDependent() && expr.EvaluateAsFloat(result, ast_context);
}

bool IsCxx11ConstantExpr(const clang::Expr& expr,
                         const clang::ASTContext& ast_context) {
  return !expr.isValueDependent() && expr.isCXX11ConstantExpr(ast_context);
}

[[nodiscard]] clang::SourceRange GetSourceRangeInMainFile(
    const clang::Preprocessor& preprocessor,
    const clang::SourceRange& source_range) {
  // Try to get the beginning of the source range in th emain file.
  const clang::SourceLocation begin_loc_in_main_file =
      getSourceLocationInMainFileForExtremeOfRange(source_range.getBegin(),
                                                   preprocessor, true);
  if (begin_loc_in_main_file.isInvalid()) {
    // The beginning of the source range could not be traced to the main file,
    // so the entire range cannot.
    return {};
  }
  // Try to get the end of the source range in th emain file.
  const clang::SourceLocation end_loc_in_main_file =
      getSourceLocationInMainFileForExtremeOfRange(source_range.getEnd(),
                                                   preprocessor, false);
  if (end_loc_in_main_file.isInvalid()) {
    // The end of the source range could not be traced to the main file, so the
    // entire range cannot.
    return {};
  }
  return {begin_loc_in_main_file, end_loc_in_main_file};
}

}  // namespace dredd
