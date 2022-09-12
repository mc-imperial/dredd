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

#include <memory>
#include <type_traits>

#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "libdredd/new_mutate_frontend_action_factory.h"
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
static llvm::cl::opt<bool> NoMutationOpts(
    "no-mutation-opts", llvm::cl::desc("Disable Dredd's optimisations"),
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
    llvm::errs() << toString(options.takeError());
    return 1;
  }

  clang::tooling::ClangTool Tool(options.get().getCompilations(),
                                 options.get().getSourcePathList());

  int mutation_id = 0;

  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      dredd::NewMutateFrontendActionFactory(!NoMutationOpts, mutation_id);

  return Tool.run(factory.get());
}
