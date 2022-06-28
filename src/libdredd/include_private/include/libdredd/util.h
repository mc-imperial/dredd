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

#include "clang/Lex/Preprocessor.h"

namespace dredd {

template <typename HasSourceRange>
[[nodiscard]] clang::SourceRange GetSourceRangeInMainFile(
    const clang::Preprocessor& preprocessor, const HasSourceRange& ast_node) {
  const clang::SourceManager& source_manager = preprocessor.getSourceManager();
  auto main_file_id = source_manager.getMainFileID();

  clang::SourceLocation begin_loc_in_main_file;
  clang::SourceLocation end_loc_in_main_file;
  clang::SourceLocation macro_expansion_location;
  {
    clang::SourceLocation begin_loc = ast_node.getSourceRange().getBegin();
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
    clang::SourceLocation end_loc = ast_node.getSourceRange().getEnd();
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

}  // namespace dredd

#endif  // LIBDREDD_UTIL_H
