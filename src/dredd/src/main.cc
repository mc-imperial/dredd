#include <random>
#include <string>

#include "clang/Basic/SourceManager.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Signals.h"

#include "libdredd/new_mutate_frontend_action_factory.h"

// Set up the command line options
static llvm::cl::extrahelp common_help(clang::tooling::CommonOptionsParser::HelpMessage);
static llvm::cl::OptionCategory mutate_category("mutate options");
static llvm::cl::opt<std::string> output_filename("o", llvm::cl::desc("Specify output filename"), llvm::cl::value_desc("filename"));
static llvm::cl::opt<std::string> seed("seed", llvm::cl::desc("Specify seed for random number generation"), llvm::cl::value_desc("seed"));
static llvm::cl::opt<std::string> num_mutations("num_mutations", llvm::cl::desc("Specify number of mutations to apply"), llvm::cl::value_desc("num_mutations"));


int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

  llvm::Expected<clang::tooling::CommonOptionsParser> options = clang::tooling::CommonOptionsParser::create(argc, argv, mutate_category, llvm::cl::OneOrMore);
  if (!options) {
    llvm::errs() << toString(options.takeError());
    return 1;
  }

  if (output_filename.empty()) {
    llvm::errs() << "Please specify an output filename using the -o option.\n";
    return 1;
  }

  if (num_mutations.empty()) {
    llvm::errs() << "Please specify a number of mutations to apply, using the -num_mutations option.\n";
    return 1;
  }

  clang::tooling::ClangTool Tool(options.get().getCompilations(), options.get().getSourcePathList());

  int seed_value = seed.empty() ? static_cast<int>(std::random_device()()) : std::stoi(seed.getValue());

  std::mt19937 generator(seed_value);

  std::unique_ptr<clang::tooling::FrontendActionFactory> factory = dredd::NewMutateFrontendActionFactory(std::stoi(num_mutations.getValue()), output_filename.getValue(), generator);

  return Tool.run(factory.get());
}
