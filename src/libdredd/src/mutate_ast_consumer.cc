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
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Type.h"
#include "clang/AST/TypeLoc.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileEntry.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/mutation.h"
#include "libdredd/util.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
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

  protobufs::MutationInfoForFile mutation_info_for_file;

  protobufs::MutationTreeNode* root_protobuf_mutation_tree_node =
      mutation_info_for_file.add_mutation_tree();
  ApplyMutations(visitor_->GetMutations(), initial_mutation_id, ast_context,
                 mutation_info_for_file, *root_protobuf_mutation_tree_node,
                 dredd_declarations, mutation_info_->has_value());

  if (initial_mutation_id == *mutation_id_) {
    // No possibilities for mutation were found; nothing else to do.
    return;
  }

  // Rewrite the size expressions of constant-sized arrays as needed.
  for (const auto& constant_sized_array_decl :
       visitor_->GetConstantSizedArraysToRewrite()) {
    assert(compiler_instance_->getLangOpts().CPlusPlus &&
           constant_sized_array_decl->getType()->isConstantArrayType());
    auto constant_array_typeloc = constant_sized_array_decl->getTypeSourceInfo()
                                      ->getTypeLoc()
                                      .getUnqualifiedLoc()
                                      .getAs<clang::ConstantArrayTypeLoc>();
    if (constant_array_typeloc.isNull()) {
      // In some cases a declaration with constant array type does not have
      // an associated ConstantArrayTypeLoc object. This happens, for example,
      // when structured bindings are used. For example, consider:
      //
      //     auto [x, y] = a;
      //
      // This yields a constant-sized array, but there is no explicit array type
      // declaration. In such cases, no action is required.
      continue;
    }
    RewriteExpressionInMainFileToIntegral(
        constant_array_typeloc.getSizeExpr(),
        llvm::dyn_cast<clang::ConstantArrayType>(
            constant_sized_array_decl->getType()->getAsArrayTypeUnsafe())
            ->getSize()
            .getLimitedValue());
  }

  if (mutation_info_->has_value()) {
    mutation_info_for_file.set_filename(
        ast_context.getSourceManager()
            .getFileEntryForID(ast_context.getSourceManager().getMainFileID())
            ->getName()
            .str());
    *mutation_info_->value().add_info_for_files() = mutation_info_for_file;
  }
  // Rewrite the argument of static assertion as needed to `1`.
  // There's no need to evaluate the actual expression, as any value other than
  // 1 would have caused the front-end used by Dredd to fail.
  for (const auto& static_assert_decl :
       visitor_->GetStaticAssertionsToRewrite()) {
    RewriteExpressionInMainFileToIntegral(static_assert_decl->getAssertExpr(),
                                          1);
  }

  // Rewrite the constant integer argument of some function.
  for (const auto* constant_argument_expresion :
       visitor_->GetConstantFunctionArgumentsToRewrite()) {
    RewriteExpressionInMainFileToIntegral(
        constant_argument_expresion,
        constant_argument_expresion
            ->getIntegerConstantExpr(compiler_instance_->getASTContext())
            .value()
            .getLimitedValue());
  }

  *mutation_info_->add_info_for_files() = mutation_info_for_file;

  const clang::SourceLocation start_location_of_first_function_in_source_file =
      visitor_->GetStartLocationOfFirstFunctionInSourceFile();
  assert(start_location_of_first_function_in_source_file.isValid() &&
         "There is at least one mutation, therefore there must be at least one "
         "function.");

  // Convert the unordered set Dredd declarations into an ordered set and add
  // them to the source file before the first declaration.
  std::set<std::string> sorted_dredd_declarations;
  sorted_dredd_declarations.insert(dredd_declarations.begin(),
                                   dredd_declarations.end());
  for (const auto& decl : sorted_dredd_declarations) {
    const bool rewriter_result = rewriter_.InsertTextBefore(
        start_location_of_first_function_in_source_file, decl);
    (void)rewriter_result;  // Keep release-mode compilers happy.
    assert(!rewriter_result && "Rewrite failed.\n");
  }

  const std::string dredd_prelude =
      compiler_instance_->getLangOpts().CPlusPlus
          ? GetDreddPreludeCpp(initial_mutation_id)
          : GetDreddPreludeC(initial_mutation_id);

  bool rewriter_result = rewriter_.InsertTextBefore(
      start_location_of_first_function_in_source_file, dredd_prelude);
  (void)rewriter_result;  // Keep release-mode compilers happy.
  assert(!rewriter_result && "Rewrite failed.\n");

  rewriter_result = rewriter_.overwriteChangedFiles();
  (void)rewriter_result;  // Keep release mode compilers happy
  assert(!rewriter_result && "Something went wrong emitting rewritten files.");
}

bool MutateAstConsumer::RewriteExpressionInMainFileToIntegral(
    const clang::Expr* expr, uint64_t x) {
  auto source_range_in_main_file =
      GetSourceRangeInMainFile(compiler_instance_->getPreprocessor(), *expr);

  // We only consider the rewriting if the source range for the size
  // expression is in the main source file.
  if (source_range_in_main_file.isValid()) {
    std::stringstream stringstream;
    stringstream << x;
    rewriter_.ReplaceText(source_range_in_main_file, stringstream.str());
    return true;
  }
  return false;
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
  result << "            enabled_bitset[local_value / 64] |= "
            "(static_cast<uint64_t>(1) << "
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
  result << "  return (enabled_bitset[local_mutation_id / 64] & "
            "(static_cast<uint64_t>(1) << "
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
  result << "static bool __dredd_enabled_mutation(int local_mutation_id) {\n";
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
  result << "          enabled_bitset[local_value / 64] |= ((uint64_t) 1 << "
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
  result
      << "  return enabled_bitset[local_mutation_id / 64] & ((uint64_t) 1 << "
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

void MutateAstConsumer::ApplyMutations(
    const MutationTreeNode& dredd_mutation_tree_node, int initial_mutation_id,
    clang::ASTContext& context,
    protobufs::MutationInfoForFile& protobufs_mutation_info_for_file,
    protobufs::MutationTreeNode& protobufs_mutation_tree_node,
    std::unordered_set<std::string>& dredd_declarations, bool build_tree) {
  assert(!(dredd_mutation_tree_node.IsEmpty() &&
           dredd_mutation_tree_node.GetChildren().size() == 1) &&
         "The mutation tree should already be compressed.");
  for (const auto& child : dredd_mutation_tree_node.GetChildren()) {
    assert(!child->IsEmpty() &&
           "The mutation tree should not have empty subtrees.");
    protobufs_mutation_tree_node.add_children(static_cast<uint32_t>(
        protobufs_mutation_info_for_file.mutation_tree_size()));
    protobufs::MutationTreeNode* new_protobufs_mutation_tree_node =
        protobufs_mutation_info_for_file.add_mutation_tree();
    ApplyMutations(
        *child, initial_mutation_id, context, protobufs_mutation_info_for_file,
        *new_protobufs_mutation_tree_node, dredd_declarations, build_tree);
  }

  for (const auto& mutation : dredd_mutation_tree_node.GetMutations()) {
    const int mutation_id_old = *mutation_id_;
    const auto mutation_group = mutation->Apply(
        context, compiler_instance_->getPreprocessor(), optimise_mutations_,
        only_track_mutant_coverage_, initial_mutation_id, *mutation_id_,
        rewriter_, dredd_declarations);
    if (build_tree && *mutation_id_ > mutation_id_old) {
      // Only add the result of applying the mutation if it had an effect.
      *protobufs_mutation_tree_node.add_mutation_groups() = mutation_group;
    }
  }
}

}  // namespace dredd
