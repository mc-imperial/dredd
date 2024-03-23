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
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileEntry.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/mutation.h"
#include "libdredd/util.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

namespace dredd {

void MutateAstConsumer::HandleTranslationUnit(clang::ASTContext& ast_context) {
  const std::string filename =
      ast_context.getSourceManager()
          .getFileEntryForID(ast_context.getSourceManager().getMainFileID())
          ->getName()
          .str();

  llvm::errs() << "Processing " << filename << "\n";

  if (ast_context.getDiagnostics().hasErrorOccurred()) {
    llvm::errs() << "Skipping due to errors\n";
    return;
  }

  if (dump_ast_) {
    llvm::errs() << "AST:\n";
    ast_context.getTranslationUnitDecl()->dump();
    llvm::errs() << "\n";
  }
  visitor_->TraverseDecl(ast_context.getTranslationUnitDecl());

  rewriter_.setSourceMgr(compiler_instance_->getSourceManager(),
                         compiler_instance_->getLangOpts());

  // Recording this makes it possible to keep track of how many mutations have
  // been applied to an individual source file, which allows mutations within
  // that source file to be tracked using a file-local mutation id that starts
  // from zero. Adding this value to the file-local id gives the global mutation
  // id.
  const int initial_mutation_id = *mutation_id_;

  // This is used to collect the various declarations that are introduced by
  // mutations in a manner that avoids duplicates, after which they can be added
  // to the start of the source file. As lots of duplicates are expected, an
  // unordered set is used to facilitate efficient lookup. Later, this is
  // converted to an ordered set so that declarations can be added to the source
  // file in a deterministic order.
  std::unordered_set<std::string> dredd_declarations;
  std::unordered_set<std::string> dredd_macros;

  std::optional<protobufs::MutationTreeNode> mutable_mutation_tree_root =
      ApplyMutations(visitor_->GetMutations(), initial_mutation_id, ast_context,
                     dredd_declarations, dredd_macros,
                     mutation_info_->has_value());

  protobufs::MutationInfoForFile mutation_info_for_file;
  if (mutable_mutation_tree_root.has_value()) {
    mutation_info_for_file.set_filename(
        ast_context.getSourceManager()
            .getFileEntryForID(ast_context.getSourceManager().getMainFileID())
            ->getName()
            .str());
    // Mutation tree root has a value if mutation_info_ has a value, so we don't
    // need to check.
    *mutation_info_for_file.mutable_mutation_tree_root() =
        mutable_mutation_tree_root.value();
  }

  if (initial_mutation_id == *mutation_id_) {
    // No possibilities for mutation were found; nothing else to do.
    return;
  }

  if (mutation_info_->has_value()) {
    *mutation_info_->value().add_info_for_files() = mutation_info_for_file;
  }

  auto& source_manager = ast_context.getSourceManager();
  const clang::SourceLocation start_of_source_file =
      source_manager.translateLineCol(source_manager.getMainFileID(), 1, 1);
  assert(start_of_source_file.isValid() &&
         "There is at least one mutation, therefore the file must have some "
         "content.");

  // Convert the unordered set Dredd declarations into an ordered set and add
  // them to the source file before the first declaration.
  std::set<std::string> sorted_dredd_declarations;
  sorted_dredd_declarations.insert(dredd_declarations.begin(),
                                   dredd_declarations.end());
  for (const auto& decl : sorted_dredd_declarations) {
    const bool rewriter_result =
        rewriter_.InsertTextBefore(start_of_source_file, decl);
    (void)rewriter_result;  // Keep release-mode compilers happy.
    assert(!rewriter_result && "Rewrite failed.\n");
  }

  rewriter_.InsertTextBefore(
      start_of_source_file,
      GenerateMutationPrelude(semantics_preserving_mutation_));

  std::set<std::string> sorted_dredd_macros;
  sorted_dredd_macros.insert(dredd_macros.begin(), dredd_macros.end());
  for (const auto& macro : sorted_dredd_macros) {
    const bool rewriter_result =
        rewriter_.InsertTextBefore(start_of_source_file, macro);
    (void)rewriter_result;  // Keep release-mode compilers happy.
    assert(!rewriter_result && "Rewrite failed.\n");
  }

  rewriter_.InsertTextBefore(
      start_of_source_file,
      GenerateMutationReturn(semantics_preserving_mutation_));

  const std::string dredd_prelude =
      compiler_instance_->getLangOpts().CPlusPlus
          ? GetDreddPreludeCpp(initial_mutation_id)
          : GetDreddPreludeC(initial_mutation_id);

  bool rewriter_result =
      rewriter_.InsertTextBefore(start_of_source_file, dredd_prelude);
  (void)rewriter_result;  // Keep release-mode compilers happy.
  assert(!rewriter_result && "Rewrite failed.\n");

  if (semantics_preserving_mutation_) {
    // TODO(JamesLeeJones): Possibly modify this variable.
    rewriter_result = rewriter_.InsertTextBefore(
        start_of_source_file, "static unsigned long long int no_op = 0;\n\n");
    (void)rewriter_result;  // Keep release-mode compilers happy.
    assert(!rewriter_result && "Rewrite failed.\n");
  }

  rewriter_result = rewriter_.overwriteChangedFiles();
  (void)rewriter_result;  // Keep release mode compilers happy
  assert(!rewriter_result && "Something went wrong emitting rewritten files.");
}

std::string MutateAstConsumer::GetRegularDreddPreludeCpp(
    int initial_mutation_id) const {
  // The number of mutations applied to this file is known.
  const int num_mutations = *mutation_id_ - initial_mutation_id;

  // Whether mutants are enabled or not will be tracked using a bitset,
  // represented as an array of 64-bit integers. First, work out how large this
  // array will need to be, as ceiling(num_mutations / 64).
  const int kWordSize = 64;
  const int num_64_bit_words_required =
      (num_mutations + kWordSize - 1) / kWordSize;

  std::stringstream result;
  result << "#include <cinttypes>\n";
  result << "#include <cstddef>\n";
  result << "#include <functional>\n";
  if (semantics_preserving_mutation_) result << "#include <limits>\n";
  result << "#include <string>\n\n";
  result << "\n";
  result << "#ifdef _MSC_VER\n";
  result << "#define thread_local __declspec(thread)\n";
  result << "#elif __APPLE__\n";
  result << "#define thread_local __thread\n";
  result << "#endif\n";
  result << "\n";
  // This allows for fast checking that at least *some* mutation in the file is
  // enabled. It is set to true initially so that __dredd_enabled_mutation gets
  // invoked the first time enabledness is queried. At that point it will get
  // set to false if no mutations are actually enabled.
  result << "static thread_local bool __dredd_some_mutation_enabled = true;\n";
  result << "static bool __dredd_enabled_mutation(int local_mutation_id) {\n";
  result << "  static thread_local bool initialized = false;\n";
  // Array of booleans, one per mutation in this file, determining whether they
  // are enabled.
  result << "  static thread_local uint64_t enabled_bitset["
         << num_64_bit_words_required << "];\n";
  result << "  if (!initialized) {\n";
  // Record locally whether some mutation is enabled.
  result << "    bool some_mutation_enabled = false;\n";
  result << "    const char* dredd_environment_variable = "
            "std::getenv(\"DREDD_ENABLED_MUTATION\");\n";
  result << "    if (dredd_environment_variable != nullptr) {\n";
  // The environment variable for mutations is set, so process the contents of
  // this environment variable as a comma-seprated list of strings.
  result << "      std::string contents(dredd_environment_variable);\n";
  result << "      while (true) {\n";
  // Find the position of the next comma.
  result << "        size_t pos = contents.find(\",\");\n";
  // The next token is either the whole string (if there is no comma) or the
  // prefix before the next comma (if there is a comma).
  result << "        std::string token = (pos == std::string::npos ? "
            "contents : contents.substr(0, pos));\n";
  // Ignore an empty token: this allows for a trailing comma at the end of the
  // string.
  result << "        if (!token.empty()) {\n";
  // Parse the token as an integer. This will throw an exception if parsing
  // fails, which is OK: it is expected that the user has set the environment
  // variable to a legitimate value.
  result << "          int value = std::stoi(token);\n";
  result << "          int local_value = value - " << initial_mutation_id
         << ";\n";
  // Check whether the mutant id actually corresponds to a mutant in this file;
  // skip it if it does not.
  result << "          if (local_value >= 0 && local_value < " << num_mutations
         << ") {\n";
  // `local_value / 64` gives the element in the bitset array corresponding to
  // this mutant. Then `local_value % 64` determines which bit of that element
  // needs to be set in order to enable the mutant, and a bitwise operation is
  // used to set the correct bit.
  result << "            enabled_bitset[local_value / 64] |= (1 << "
            "(local_value % 64));\n";
  // Note that at least one enabled mutation has been encountered.
  result << "            some_mutation_enabled = true;\n";
  result << "          }\n";
  result << "        }\n";
  // If the end of the string has been reached, exit the parsing loop.
  result << "        if (pos == std::string::npos) {\n";
  result << "          break;\n";
  result << "        }\n";
  // Move past the first comma so that the rest of the string can be processed.
  result << "        contents.erase(0, pos + 1);\n";
  result << "      }\n";
  result << "    }\n";
  // Initialisation is now complete, and whether at least one mutation is
  // enabled is known.
  result << "    initialized = true;\n";
  result << "    __dredd_some_mutation_enabled = some_mutation_enabled;\n";
  result << "  }\n";
  // Similar to the above, a combination of division, modulo and bit-shifting
  // is used to look up whether this mutant is enabled in the bitset.
  result << "  return (enabled_bitset[local_mutation_id / 64] & (1 << "
            "(local_mutation_id % 64))) != 0;\n";
  result << "}\n\n";
  return result.str();
}

std::string MutateAstConsumer::GetMutantTrackingDreddPreludeCpp(
    int initial_mutation_id) const {
  // The number of mutations applied to this file is known.
  const int num_mutations = *mutation_id_ - initial_mutation_id;

  std::stringstream result;
  result << "#include <atomic>\n";
  result << "#include <fstream>\n";
  result << "#include <functional>\n";
  result << "#include <sstream>\n";
  result << "\n";
  result << "static void __dredd_record_covered_mutants(int local_mutation_id, "
            "int num_mutations) {\n";
  result << "  static std::atomic<bool> already_recorded[" << num_mutations
         << "];\n";
  result
      << "  if (already_recorded[local_mutation_id].exchange(true)) return;\n";
  result << "  const char* dredd_tracking_environment_variable = "
            "std::getenv(\"DREDD_MUTANT_TRACKING_FILE\");\n";
  result << "  if (dredd_tracking_environment_variable == nullptr) return;\n";
  result << "  std::ofstream output_file;\n";
  result << "  output_file.open(dredd_tracking_environment_variable, "
            "std::ios_base::app);\n";
  result << "  for (int i = 0; i < num_mutations; i++) {\n";
  result << "    output_file << (" << std::to_string(initial_mutation_id)
         << " + local_mutation_id + i) << \"\\n\";\n";
  result << "  }\n";
  result << "}\n\n";
  return result.str();
}

std::string MutateAstConsumer::GetDreddPreludeCpp(
    int initial_mutation_id) const {
  if (only_track_mutant_coverage_) {
    return GetMutantTrackingDreddPreludeCpp(initial_mutation_id);
  }
  return GetRegularDreddPreludeCpp(initial_mutation_id);
}

std::string MutateAstConsumer::GetRegularDreddPreludeC(
    int initial_mutation_id) const {
  // See comments in GetRegularDreddPreludeCpp - this C version is a
  // straightforward port.
  const int num_mutations = *mutation_id_ - initial_mutation_id;
  const int kWordSize = 64;
  const int num_64_bit_words_required =
      (num_mutations + kWordSize - 1) / kWordSize;

  std::stringstream result;
  result << "#include <inttypes.h>\n";
  result << "#include <stdbool.h>\n";
  result << "#include <stdlib.h>\n";
  result << "#include <string.h>\n";
  result << "\n";
  result << "#ifdef _MSC_VER\n";
  result << "#define thread_local __declspec(thread)\n";
  result << "#elif __APPLE__\n";
  result << "#define thread_local __thread\n";
  result << "#else\n";
  result << "#include <threads.h>\n";
  result << "#endif\n";
  result << "\n";
  result << "static thread_local int __dredd_some_mutation_enabled = 1;\n";
  result << "static int __dredd_enabled_mutation(int local_mutation_id) {\n";
  result << "  static thread_local int initialized = 0;\n";
  result << "  static thread_local uint64_t enabled_bitset["
         << num_64_bit_words_required << "];\n";
  result << "  if (!initialized) {\n";
  result << "    int some_mutation_enabled = 0;\n";
  result << "    const char* dredd_environment_variable = "
            "getenv(\"DREDD_ENABLED_MUTATION\");\n";
  result << "    if (dredd_environment_variable) {\n";
  result
      << "      char* temp = malloc(strlen(dredd_environment_variable) + 1);\n";
  result << "      strcpy(temp, dredd_environment_variable);\n";
  result << "      char* token;\n";
  result << "      token = strtok(temp, \",\");\n";
  result << "      while(token) {\n";
  result << "        int value = atoi(token);\n";
  result << "        int local_value = value - " << initial_mutation_id
         << ";\n";
  result << "        if (local_value >= 0 && local_value < " << num_mutations
         << ") {\n";
  result << "          enabled_bitset[local_value / 64] |= (1 << "
            "(local_value % 64));\n";
  result << "          some_mutation_enabled = 1;\n";
  result << "        }\n";
  result << "        token = strtok(NULL, \",\");\n";
  result << "      }\n";
  result << "      free(temp);\n";
  result << "    }\n";
  result << "    initialized = 1;\n";
  result << "    __dredd_some_mutation_enabled = some_mutation_enabled;\n";
  result << "  }\n";
  result << "  return enabled_bitset[local_mutation_id / 64] & (1 << "
            "(local_mutation_id % 64));\n";
  result << "}\n\n";
  return result.str();
}

std::string MutateAstConsumer::GetMutantTrackingDreddPreludeC(
    int initial_mutation_id) const {
  // See comments in GetMutantTrackingDreddPreludeCpp; this is a straightforward
  // port to C.
  const int num_mutations = *mutation_id_ - initial_mutation_id;
  std::stringstream result;
  result << "#include <inttypes.h>\n";
  result << "#include <stdatomic.h>\n";
  result << "#include <stdbool.h>\n";
  result << "#include <stdio.h>\n";
  result << "#include <stdlib.h>\n";
  result << "\n";
  result << "static void __dredd_record_covered_mutants(int local_mutation_id, "
            "int num_mutations) {\n";
  result << "  static atomic_bool already_recorded[" << num_mutations << "];\n";
  result << "  if (atomic_exchange(&already_recorded[local_mutation_id], 1)) "
            "return;\n";
  result << "  const char* dredd_tracking_environment_variable = "
            "getenv(\"DREDD_MUTANT_TRACKING_FILE\");\n";
  result << "  if (!dredd_tracking_environment_variable) return;\n";
  result << "  FILE* fp = fopen(dredd_tracking_environment_variable, \"a\");\n";
  result << "  for (int i = 0; i < num_mutations; i++) {\n";
  result << R"(    fprintf(fp, "%d\n", )" + std::to_string(initial_mutation_id)
         << " + local_mutation_id + i);\n";
  result << "  }\n";
  result << "  fclose(fp);\n";
  result << "}\n\n";
  return result.str();
}

std::string MutateAstConsumer::GetDreddPreludeC(int initial_mutation_id) const {
  if (only_track_mutant_coverage_) {
    return GetMutantTrackingDreddPreludeC(initial_mutation_id);
  }
  return GetRegularDreddPreludeC(initial_mutation_id);
}

std::optional<protobufs::MutationTreeNode> MutateAstConsumer::ApplyMutations(
    const MutationTreeNode& mutation_tree_node, int initial_mutation_id,
    clang::ASTContext& context,
    std::unordered_set<std::string>& dredd_declarations,
    std::unordered_set<std::string>& dredd_macros, bool build_tree) {
  assert(!(mutation_tree_node.IsEmpty() &&
           mutation_tree_node.GetChildren().size() == 1) &&
         "The mutation tree should already be compressed.");
  protobufs::MutationTreeNode result;

  for (const auto& child : mutation_tree_node.GetChildren()) {
    assert(!child->IsEmpty() &&
           "The mutation tree should not have empty subtrees.");
    std::optional<protobufs::MutationTreeNode> children =
        ApplyMutations(*child, initial_mutation_id, context, dredd_declarations,
                       dredd_macros, build_tree);
    // children can only have a value if result has a value, so we don't need to
    // check both.
    if (children.has_value()) {
      *result.add_children() = children.value();
    }
  }
  for (const auto& mutation : mutation_tree_node.GetMutations()) {
    const int mutation_id_old = *mutation_id_;
    const auto mutation_group = mutation->Apply(
        context, compiler_instance_->getPreprocessor(), optimise_mutations_,
        semantics_preserving_mutation_, only_track_mutant_coverage_,
        initial_mutation_id, *mutation_id_, rewriter_, dredd_declarations,
        dredd_macros);
    if (build_tree && *mutation_id_ > mutation_id_old) {
      // Only add the result of applying the mutation if it had an effect.
      *result.add_mutation_groups() = mutation_group;
    }
  }
  return result;
}

}  // namespace dredd
