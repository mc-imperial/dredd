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

#include <cassert>
#include <fstream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Frontend/ChainedDiagnosticConsumer.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "dredd/log_failed_files_diagnostic_consumer.h"
#include "dredd/protobufs/protobuf_serialization.h"
#include "libdredd/new_mutate_frontend_action_factory.h"
#include "libdredd/options.h"
#include "libdredd/protobufs/dredd_protobufs.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/raw_ostream.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#elif defined(_MSC_VER)
#pragma warning(push)
#endif

// Set up the command line options
// NOLINTNEXTLINE
static llvm::cl::extrahelp common_help(
    clang::tooling::CommonOptionsParser::HelpMessage);
// NOLINTNEXTLINE
static llvm::cl::OptionCategory mutate_category("mutate options");
// NOLINTNEXTLINE
static llvm::cl::opt<bool> no_mutation_opts(
    "no-mutation-opts", llvm::cl::desc("Disable Dredd's optimisations"),
    llvm::cl::cat(mutate_category));
static llvm::cl::opt<bool>
    // NOLINTNEXTLINE
    opt_do_not_remove_side_effect_free_expression_statements(
        "opt-do-not-remove-side-effect-free-expression-statements",
        llvm::cl::desc("TODO"), llvm::cl::cat(mutate_category));
// NOLINTNEXTLINE
static llvm::cl::opt<bool> opt_do_not_remove_compound_statements(
    "opt-do-not-remove-compound-statements", llvm::cl::desc("TODO"),
    llvm::cl::cat(mutate_category));
// NOLINTNEXTLINE
static llvm::cl::opt<bool> opt_do_not_mutate_casts_cleanups_and_parentheses(
    "opt-do-not-mutate-casts-cleanups-and-parentheses", llvm::cl::desc("TODO"),
    llvm::cl::cat(mutate_category));
// NOLINTNEXTLINE
static llvm::cl::opt<bool> opt_leverage_constant_folding(
    "opt-leverage-constant-folding", llvm::cl::desc("TODO"),
    llvm::cl::cat(mutate_category));
// NOLINTNEXTLINE
static llvm::cl::opt<bool> opt_do_not_replace_relational_with_argument(
    "opt-do-not-replace-relational-with-argument", llvm::cl::desc("TODO"),
    llvm::cl::cat(mutate_category));
// NOLINTNEXTLINE
static llvm::cl::opt<bool> opt_avoid_redundant_operator_mutation_combinations(
    "opt-avoid-redundant-operator-mutation-combinations",
    llvm::cl::desc("TODO"), llvm::cl::cat(mutate_category));
// NOLINTNEXTLINE
static llvm::cl::opt<bool> opt_avoid_self_inverse_unary_operator_removal(
    "opt-avoid-self-inverse-unary-operator-removal", llvm::cl::desc("TODO"),
    llvm::cl::cat(mutate_category));
// NOLINTNEXTLINE
static llvm::cl::opt<bool> opt_do_not_mutate_sizeof_and_alignof(
    "opt-do-not-mutate-sizeof-and-alignof",
    llvm::cl::desc("Do not mutate arguments to 'sizeof' and 'alignof'"),
    llvm::cl::cat(mutate_category));
// NOLINTNEXTLINE
static llvm::cl::opt<bool> only_track_mutant_coverage(
    "only-track-mutant-coverage",
    llvm::cl::desc("Add instrumentation to track which mutants are covered by "
                   "an input, rather than actually applying any mutants."),
    llvm::cl::cat(mutate_category));
// NOLINTNEXTLINE
static llvm::cl::opt<bool> dump_asts(
    "dump-asts",
    llvm::cl::desc("Dump each AST that is processed; useful for debugging"),
    llvm::cl::cat(mutate_category));
// NOLINTNEXTLINE
static llvm::cl::opt<std::string> mutation_info_file(
    "mutation-info-file",
    llvm::cl::desc(
        ".json file into which mutation information should be written"),
    llvm::cl::cat(mutate_category));
// NOLINTNEXTLINE
static llvm::cl::opt<bool> show_ast_node_types(
    "show-ast-node-types",
    llvm::cl::desc(
        "In the mutated code, show (via comments) the type of each AST node to "
        "which mutation has been applied; useful for debugging"),
    llvm::cl::cat(mutate_category));
static llvm::cl::opt<dredd::Options::EnablednessCheckingMode>
    // NOLINTNEXTLINE
    enabledness_checking_mode(
        "enabledness-checking-mode",
        llvm::cl::desc("Method for enabledness checking (default: STANDARD)"),
        llvm::cl::init(dredd::Options::EnablednessCheckingMode::STANDARD),
        llvm::cl::values(
            clEnumValN(dredd::Options::EnablednessCheckingMode::STANDARD,
                       "STANDARD", "Default and recommended"),
            clEnumValN(
                dredd::Options::EnablednessCheckingMode::NO_SOME_ENABLED_CHECK,
                "NO_SOME_ENABLED_CHECK",
                "Disable pre-testing of whether any mutants are enabled "
                "for a file"),
            clEnumValN(
                dredd::Options::EnablednessCheckingMode::
                    ALWAYS_READ_ENVIRONMENT_VARIABLE,
                "ALWAYS_READ_ENVIRONMENT_VARIABLE",
                "Read the environment variable on every mutant enabledness "
                "check; for evaluation purposes only")),
        llvm::cl::cat(mutate_category));

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace {
[[nodiscard]] dredd::Options::Optimisations GetOptimisations() {
  bool specific_optimisations_enabled = false;
  bool enabled_do_not_remove_side_effect_free_expression_statements = false;
  bool enabled_do_not_mutate_sizeof_and_alignof = false;
  if (opt_do_not_remove_side_effect_free_expression_statements) {
    specific_optimisations_enabled = true;
    enabled_do_not_remove_side_effect_free_expression_statements = true;
  }
  bool enabled_do_not_remove_compound_statements = false;
  if (opt_do_not_remove_compound_statements) {
    specific_optimisations_enabled = true;
    enabled_do_not_remove_compound_statements = true;
  }
  bool enabled_do_not_mutate_casts_cleanups_and_parentheses = false;
  if (opt_do_not_mutate_casts_cleanups_and_parentheses) {
    specific_optimisations_enabled = true;
    enabled_do_not_mutate_casts_cleanups_and_parentheses = true;
  }
  bool enabled_leverage_constant_folding = false;
  if (opt_leverage_constant_folding) {
    specific_optimisations_enabled = true;
    enabled_leverage_constant_folding = true;
  }
  bool enabled_do_not_replace_relational_with_argument = false;
  if (opt_do_not_replace_relational_with_argument) {
    specific_optimisations_enabled = true;
    enabled_do_not_replace_relational_with_argument = true;
  }
  bool enabled_avoid_redundant_operator_mutation_combinations = false;
  if (opt_avoid_redundant_operator_mutation_combinations) {
    specific_optimisations_enabled = true;
    enabled_avoid_redundant_operator_mutation_combinations = true;
  }
  bool enabled_avoid_self_inverse_unary_operator_removal = false;
  if (opt_avoid_self_inverse_unary_operator_removal) {
    specific_optimisations_enabled = true;
    enabled_avoid_self_inverse_unary_operator_removal = true;
  }
  if (opt_do_not_mutate_sizeof_and_alignof) {
    specific_optimisations_enabled = true;
    enabled_do_not_mutate_sizeof_and_alignof = true;
  }
  if (specific_optimisations_enabled) {
    return dredd::Options::Optimisations(
        enabled_do_not_remove_side_effect_free_expression_statements,
        enabled_do_not_remove_compound_statements,
        enabled_do_not_mutate_casts_cleanups_and_parentheses,
        enabled_leverage_constant_folding,
        enabled_do_not_replace_relational_with_argument,
        enabled_avoid_redundant_operator_mutation_combinations,
        enabled_avoid_self_inverse_unary_operator_removal,
        enabled_do_not_mutate_sizeof_and_alignof);
  }
  return no_mutation_opts ? dredd::Options::Optimisations::AllDisabled()
                          : dredd::Options::Optimisations::AllEnabled();
}
}  // namespace

int main(int argc, const char** argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

  llvm::Expected<clang::tooling::CommonOptionsParser> command_line_options =
      clang::tooling::CommonOptionsParser::create(argc, argv, mutate_category,
                                                  llvm::cl::OneOrMore);
  if (!command_line_options) {
    const std::string error_message =
        toString(command_line_options.takeError());
    llvm::errs() << error_message;
    return 1;
  }

  clang::tooling::ClangTool tool(
      command_line_options.get().getCompilations(),
      command_line_options.get().getSourcePathList());

  const llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> diagnostic_options =
      new clang::DiagnosticOptions();
  diagnostic_options->ShowColors = 1;
  std::unique_ptr<clang::TextDiagnosticPrinter> text_diagnostic_printer =
      std::make_unique<clang::TextDiagnosticPrinter>(llvm::errs(),
                                                     &*diagnostic_options);
  const clang::TextDiagnosticPrinter* text_diagnostic_printer_ptr =
      text_diagnostic_printer.get();
  std::unique_ptr<LogFailedFilesDiagnosticConsumer>
      log_failed_files_diagnostic_consumer =
          std::make_unique<LogFailedFilesDiagnosticConsumer>();
  const LogFailedFilesDiagnosticConsumer*
      log_failed_files_diagnostic_consumer_ptr =
          log_failed_files_diagnostic_consumer.get();
  clang::ChainedDiagnosticConsumer chained_diagnostic_consumer(
      std::move(text_diagnostic_printer),
      std::move(log_failed_files_diagnostic_consumer));
  tool.setDiagnosticConsumer(&chained_diagnostic_consumer);

  // Used to give each mutation a unique identifier.
  int mutation_id = 0;

  // Keeps track of the mutations that are applied to each source file,
  // including their hierarchical structure.
  std::optional<dredd::protobufs::MutationInfo> mutation_info;

  if (mutation_info_file.empty()) {
    mutation_info = std::nullopt;
  } else {
    mutation_info = dredd::protobufs::MutationInfo();
  }

  const dredd::Options dredd_options(
      GetOptimisations(), dump_asts, only_track_mutant_coverage,
      show_ast_node_types, enabledness_checking_mode);

  const std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      dredd::NewMutateFrontendActionFactory(dredd_options, mutation_id,
                                            mutation_info);

  const int return_code = tool.run(factory.get());

  if (return_code == 0) {
    // Keep release mode compilers happy.
    (void)text_diagnostic_printer_ptr;
    assert(text_diagnostic_printer_ptr->getNumErrors() == 0);
    assert(
        log_failed_files_diagnostic_consumer_ptr->GetFilesWithErrors().empty());
  } else {
    assert(text_diagnostic_printer_ptr->getNumErrors() > 0);
    if (!log_failed_files_diagnostic_consumer_ptr->GetFilesWithErrors()
             .empty()) {
      llvm::errs()
          << "The following files were not mutated due to compile-time "
             "errors; see above for details:\n";
      for (const auto& file :
           log_failed_files_diagnostic_consumer_ptr->GetFilesWithErrors()) {
        llvm::errs() << "  " << file << "\n";
      }
    }
  }

  if (mutation_info.has_value()) {
    // Write out the mutation info in JSON format for those files that were
    // successfully mutated.
    std::string json_string;
    auto json_options = google::protobuf::util::JsonOptions();
    json_options.add_whitespace = true;
    json_options.always_print_primitive_fields = true;
    auto json_generation_status = google::protobuf::util::MessageToJsonString(
        mutation_info.value(), &json_string, json_options);
    if (json_generation_status.ok()) {
      std::ofstream transformations_json_file(mutation_info_file);
      transformations_json_file << json_string;
    } else {
      llvm::errs() << "Error writing JSON data to " << mutation_info_file
                   << "\n";
      return 1;
    }
  }
  return return_code;
}
