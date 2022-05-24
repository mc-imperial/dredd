#include "libdredd/new_mutate_frontend_action_factory.h"

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "llvm/ADT/StringRef.h"

#include "libdredd/mutate_ast_consumer.h"

namespace dredd {

class MutateFrontendAction : public clang::ASTFrontendAction {
public:
  MutateFrontendAction(size_t num_mutations, const std::string& output_file, std::mt19937& generator) : num_mutations_(num_mutations), output_file_(output_file), generator_(generator) { }

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef file) override {
    (void) file;
    return std::make_unique<MutateAstConsumer>(ci, num_mutations_, output_file_, generator_);
  }

private:
  const size_t num_mutations_;
  const std::string& output_file_;
  std::mt19937& generator_;
};

std::unique_ptr<clang::tooling::FrontendActionFactory> NewMutateFrontendActionFactory(size_t num_mutations, const std::string& output_filename, std::mt19937& generator) {
  class MutateFrontendActionFactory : public clang::tooling::FrontendActionFactory {
  public:
    MutateFrontendActionFactory(size_t num_mutations, const std::string& output_filename, std::mt19937& generator) : num_mutations_(num_mutations), output_filename_(output_filename), generator_(generator) {}

    std::unique_ptr<clang::FrontendAction> create() override {
      return std::make_unique<MutateFrontendAction>(num_mutations_, output_filename_, generator_);
    }

  private:
    size_t num_mutations_;
    const std::string& output_filename_;
    std::mt19937& generator_;
  };

  return std::make_unique<MutateFrontendActionFactory>(
      num_mutations, output_filename, generator);
}

}  // dredd