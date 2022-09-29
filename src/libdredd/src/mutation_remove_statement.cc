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

#include <cassert>
#include <string>
#include <unordered_set>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/TokenKinds.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/Transformer/SourceCode.h"
#include "libdredd/util.h"

namespace dredd {

MutationRemoveStatement::MutationRemoveStatement(const clang::Stmt& statement)
    : statement_(statement) {}

void MutationRemoveStatement::Apply(
    clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
    bool optimise_mutations, int first_mutation_id_in_file, int& mutation_id,
    clang::Rewriter& rewriter,
    std::unordered_set<std::string>& dredd_declarations) const {
  (void)dredd_declarations;  // Unused.
  (void)optimise_mutations;  // Unused.
  clang::CharSourceRange source_range = clang::CharSourceRange::getTokenRange(
      GetSourceRangeInMainFile(preprocessor, statement_));

  // If the statement is followed immediately by a semi-colon, possibly with
  // intervening comments, that semi-colon should be part of the code that is
  // wrapped in an 'if'.

  // First, skip over any intervening comments. It doesn't matter whether or not
  // they end up getting wrapped in the 'if'.
  bool is_extended_with_comment = false;
  while (true) {
    // Extend the source range in the case that the next token is a comment.
    clang::CharSourceRange source_range_extended_with_comment =
        clang::tooling::maybeExtendRange(
            source_range, clang::tok::TokenKind::comment, ast_context);
    if (source_range.getAsRange() ==
        source_range_extended_with_comment.getAsRange()) {
      // The source range wasn't extended, so there aren't any more comments
      // before the next non-comment token.
      break;
    }
    // Update the source range to its extended form and note that a comment was
    // skipped.
    is_extended_with_comment = true;
    source_range = source_range_extended_with_comment;
  }

  // Now try to extend the source range further to include the next token, if it
  // is a semi-colon.
  clang::CharSourceRange source_range_extended_with_semi =
      clang::tooling::maybeExtendRange(
          source_range, clang::tok::TokenKind::semi, ast_context);
  bool is_extended_with_semi =
      source_range.getAsRange() != source_range_extended_with_semi.getAsRange();
  if (is_extended_with_semi) {
    source_range = source_range_extended_with_semi;
  }

  // Subtracting |first_mutation_id_in_file| turns the global mutation id,
  // |mutation_id|, into a file-local mutation id.
  const int local_mutation_id = mutation_id - first_mutation_id_in_file;
  bool rewriter_result = rewriter.InsertTextBefore(
      source_range.getBegin(), "if (!__dredd_enabled_mutation(" +
                                   std::to_string(local_mutation_id) + ")) { ");
  assert(!rewriter_result && "Rewrite failed.\n");
  rewriter_result = rewriter.InsertTextAfterToken(
      source_range.getEnd(),
      // If the source range was extended with a comment but not with a
      // semi-colon, it is possible that the end of the source range is on
      // the same line as a single-line comment, in which case it's
      // important to take a new line (otherwise the closing brace will form
      // part of the comment). It would be possible to always take a new
      // line, but this would make mutated files harder to read.
      std::string(
          ((is_extended_with_comment && !is_extended_with_semi) ? "\n" : " ")) +
          "}");
  assert(!rewriter_result && "Rewrite failed.\n");
  (void)rewriter_result;  // Keep release-mode compilers happy.
  mutation_id++;
}

}  // namespace dredd
