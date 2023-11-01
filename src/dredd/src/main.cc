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

#include <fstream>
#include <memory>
#include <string>
#include <type_traits>

#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "dredd/protobufs/protobuf_serialization.h"
#include "libdredd/new_mutate_frontend_action_factory.h"
#include "libdredd/protobufs/dredd_protobufs.h"
#include "llvm/ADT/ArrayRef.h"
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
// NOLINTNEXTLINE
static llvm::cl::opt<bool> only_track_mutant_coverage(
    "only-track-mutant-coverage",
    llvm::cl::desc("Add instrumentation to track which mutants are covered by "
                   "an input, rather than actually applying any mutants."),
    llvm::cl::cat(mutate_category));
// NOLINTNEXTLINE
static llvm::cl::opt<bool> semantics_preserving_coverage_instrumentation(
        "semantics-preserving-coverage-instrumentation",
        llvm::cl::desc("Apply a semantics preserving transformation to the source code "
                       "which adds extra code coverage points."),
        llvm::cl::cat(mutate_category)
        );
// NOLINTNEXTLINE
static llvm::cl::opt<bool> dump_asts(
    "dump-asts",
    llvm::cl::desc("Dump each AST that is processed; useful for debugging"),
    llvm::cl::cat(mutate_category));
// NOLINTNEXTLINE
static llvm::cl::opt<std::string> mutation_info_file(
    "mutation-info-file", llvm::cl::Required,
    llvm::cl::desc(
        ".json file into which mutation information should be written"),
    llvm::cl::cat(mutate_category));

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

int main(int argc, const char** argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

  llvm::Expected<clang::tooling::CommonOptionsParser> options =
      clang::tooling::CommonOptionsParser::create(argc, argv, mutate_category,
                                                  llvm::cl::OneOrMore);
  if (!options) {
    const std::string error_message = toString(options.takeError());
    llvm::errs() << error_message;
    return 1;
  }

  clang::tooling::ClangTool Tool(options.get().getCompilations(),
                                 options.get().getSourcePathList());

  // Used to give each mutation a unique identifier.
  int mutation_id = 0;

  // Keeps track of the mutations that are applied to each source file,
  // including their hierarchical structure.
  dredd::protobufs::MutationInfo mutation_info;

  const std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      dredd::NewMutateFrontendActionFactory(!no_mutation_opts, dump_asts,
                                            only_track_mutant_coverage,
                                            semantics_preserving_coverage_instrumentation,
                                            mutation_id, mutation_info);

  const int return_code = Tool.run(factory.get());

  if (return_code == 0) {
    // Application of mutations was successful, so write out the mutation info
    // in JSON format.
    std::string json_string;
    auto json_options = google::protobuf::util::JsonOptions();
    json_options.add_whitespace = true;
    json_options.always_print_primitive_fields = true;
    auto json_generation_status = google::protobuf::util::MessageToJsonString(
        mutation_info, &json_string, json_options);
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
