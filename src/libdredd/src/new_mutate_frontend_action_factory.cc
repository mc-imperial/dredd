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

#include "libdredd/new_mutate_frontend_action_factory.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include "libdredd/mutate_ast_consumer.h"
#include "llvm/ADT/StringRef.h"

namespace dredd {

class MutateFrontendAction : public clang::ASTFrontendAction {
 public:
  MutateFrontendAction(size_t num_mutations, const std::string& output_file,
                       RandomGenerator& generator)
      : num_mutations_(num_mutations),
        output_file_(output_file),
        generator_(generator) {}

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& ci, llvm::StringRef file) override;

 private:
  const size_t num_mutations_;
  const std::string& output_file_;
  RandomGenerator& generator_;
};

std::unique_ptr<clang::tooling::FrontendActionFactory>
NewMutateFrontendActionFactory(size_t num_mutations,
                               const std::string& output_filename,
                               RandomGenerator& generator) {
  class MutateFrontendActionFactory
      : public clang::tooling::FrontendActionFactory {
   public:
    MutateFrontendActionFactory(size_t num_mutations,
                                const std::string& output_filename,
                                RandomGenerator& generator)
        : num_mutations_(num_mutations),
          output_filename_(output_filename),
          generator_(generator) {}

    std::unique_ptr<clang::FrontendAction> create() override {
      return std::make_unique<MutateFrontendAction>(
          num_mutations_, output_filename_, generator_);
    }

   private:
    size_t num_mutations_;
    const std::string& output_filename_;
    RandomGenerator& generator_;
  };

  return std::make_unique<MutateFrontendActionFactory>(
      num_mutations, output_filename, generator);
}

std::unique_ptr<clang::ASTConsumer> MutateFrontendAction::CreateASTConsumer(
    clang::CompilerInstance& ci, llvm::StringRef file) {
  (void)file;
  return std::make_unique<MutateAstConsumer>(ci, num_mutations_, output_file_,
                                             generator_);
}

}  // namespace dredd
