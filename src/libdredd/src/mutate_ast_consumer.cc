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

#include "libdredd/mutate_ast_consumer.h"

#include <cassert>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/mutation.h"

namespace dredd {

void MutateAstConsumer::HandleTranslationUnit(clang::ASTContext& context) {
  if (context.getDiagnostics().hasErrorOccurred()) {
    // There has been an error, so we don't do any processing.
    return;
  }
  visitor_->TraverseDecl(context.getTranslationUnitDecl());

  if (visitor_->GetMutations().empty()) {
    // No possibilities for mutation were found; nothing to do.
    return;
  }

  rewriter_.setSourceMgr(compiler_instance_.getSourceManager(),
                         compiler_instance_.getLangOpts());

  // Recording this makes it possible to keep track of how many mutations have
  // been applied to an individual source file, which allows mutations within
  // that source file to be tracked using a file-local mutation id that starts
  // from zero. Adding this value to the file-local id gives the global mutation
  // id.
  const int initial_mutation_id = mutation_id_;

  // This is used to collect the various declarations that are introduced by
  // mutations in a manner that avoids duplicates, after which they can be added
  // to the start of the source file. As lots of duplicates are expected, an
  // unordered set is used to facilitate efficient lookup. Later, this is
  // converted to an ordered set so that declarations can be added to the source
  // file in a deterministic order.
  std::unordered_set<std::string> dredd_declarations;

  // By construction, mutations are processed in a bottom-up fashion. This
  // property should be preserved when the specific mutations are made at
  // random, to avoid attempts to rewrite a child node after its parent has been
  // rewritten.
  for (const auto& mutation : visitor_->GetMutations()) {
    int mutation_id_old = mutation_id_;
    mutation->Apply(context, compiler_instance_.getPreprocessor(),
                    initial_mutation_id, mutation_id_, rewriter_,
                    dredd_declarations);
    assert(mutation_id_ > mutation_id_old &&
           "Every mutation should lead to the mutation id increasing by at "
           "least 1.");
    (void)mutation_id_old;  // Keep release-mode compilers happy.
  }

  assert(visitor_->GetFirstDeclInSourceFile() != nullptr &&
         "There is at least one mutation, therefore there must be at least one "
         "declaration.");

  // Convert the unordered set Dredd declarations into an ordered set and add
  // them to the source file before the first declaration.
  std::set<std::string> sorted_dredd_declarations;
  sorted_dredd_declarations.insert(dredd_declarations.begin(),
                                   dredd_declarations.end());
  for (const auto& decl : sorted_dredd_declarations) {
    bool result = rewriter_.InsertTextBefore(
        visitor_->GetFirstDeclInSourceFile()->getBeginLoc(), decl);
    (void)result;  // Keep release-mode compilers happy.
    assert(!result && "Rewrite failed.\n");
  }

  // The number of mutations applied to this file is now known.
  const int num_mutations = mutation_id_ - initial_mutation_id;

  // Whether mutants are enabled or not will be tracked using a a bitset,
  // represented as an array of 64-bit integers. First, work out how large this
  // array will need to be, as ceiling(num_mutations / 64).
  const int kWordSize = 64;
  const int num_64_bit_words_required =
      (num_mutations + kWordSize - 1) / kWordSize;

  std::stringstream dredd_prelude;
  dredd_prelude << "#include <cinttypes>\n";
  dredd_prelude << "#include <cstddef>\n";
  dredd_prelude << "#include <functional>\n";
  dredd_prelude << "#include <string>\n\n";
  dredd_prelude
      << "static bool __dredd_enabled_mutation(int local_mutation_id) {\n";
  dredd_prelude << "  static bool initialized = false;\n";
  // Array of booleans, one per mutation in this file, determining whether they
  // are enabled.
  dredd_prelude << "  static uint64_t enabled_bitset["
                << num_64_bit_words_required << "];\n";
  dredd_prelude << "  if (!initialized) {\n";
  dredd_prelude << "    const char* dredd_environment_variable = "
                   "std::getenv(\"DREDD_ENABLED_MUTATION\");\n";
  dredd_prelude << "    if (dredd_environment_variable != nullptr) {\n";
  // The environment variable for mutations is set, so process the contents of
  // this environment variable as a comma-seprated list of strings.
  dredd_prelude << "      std::string contents(dredd_environment_variable);\n";
  dredd_prelude << "      while (true) {\n";
  // Find the position of the next comma.
  dredd_prelude << "        size_t pos = contents.find(\",\");\n";
  // The next token is either the whole string (if there is no comma) or the
  // prefix before the next comma (if there is a comma).
  dredd_prelude << "        std::string token = (pos == std::string::npos ? "
                   "contents : contents.substr(0, pos));\n";
  // Ignore an empty token: this allows for a trailing comma at the end of the
  // string.
  dredd_prelude << "        if (!token.empty()) {\n";
  // Parse the token as an integer. This will throw an exception if parsing
  // fails, which is OK: it is expected that the user has set the environment
  // variable to a legitimate value.
  dredd_prelude << "          int value = std::stoi(token);\n";
  dredd_prelude << "          int local_value = value - " << initial_mutation_id
                << ";\n";
  // Check whether the mutant id actually corresponds to a mutant in this file;
  // skip it if it does not.
  dredd_prelude << "          if (local_value >= 0 && local_value < "
                << num_mutations << ") {\n";
  // `local_value / 64` gives the element in the bitset array corresponding to
  // this mutant. Then `local_value % 64` determines which bit of that element
  // needs to be set in order to enable the mutant, and a bitwise operation is
  // used to set the correct bit.
  dredd_prelude << "            enabled_bitset[local_value / 64] |= (1 << "
                   "(local_value % 64));\n";
  dredd_prelude << "          }\n";
  dredd_prelude << "        }\n";
  // If the end of the string has been reached, exit the parsing loop.
  dredd_prelude << "        if (pos == std::string::npos) {\n";
  dredd_prelude << "          break;\n";
  dredd_prelude << "        }\n";
  // Move past the first comma so that the rest of the string can be processed.
  dredd_prelude << "        contents.erase(0, pos + 1);\n";
  dredd_prelude << "      }\n";
  dredd_prelude << "    }\n";
  dredd_prelude << "    initialized = true;\n";
  dredd_prelude << "  }\n";
  // Similar to the above, a combination of division, modulo and bit-shifting
  // is used to look up whether this mutant is enabled in the bitset.
  dredd_prelude << "  return (enabled_bitset[local_mutation_id / 64] & (1 << "
                   "(local_mutation_id % 64))) != 0;\n";
  dredd_prelude << "}\n\n";

  bool result = rewriter_.InsertTextBefore(
      visitor_->GetFirstDeclInSourceFile()->getBeginLoc(), dredd_prelude.str());
  (void)result;  // Keep release-mode compilers happy.
  assert(!result && "Rewrite failed.\n");

  result = rewriter_.overwriteChangedFiles();
  (void)result;  // Keep release mode compilers happy
  assert(!result && "Something went wrong emitting rewritten files.");
}

}  // namespace dredd
