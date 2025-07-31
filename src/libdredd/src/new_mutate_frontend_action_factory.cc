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

#include <cassert>
#include <set>
#include <string>

#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/FrontendOptions.h"
#include "clang/Tooling/Tooling.h"
#include "libdredd/mutate_ast_consumer.h"
#include "libdredd/options.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

namespace dredd {

class MutateFrontendAction : public clang::ASTFrontendAction {
 public:
  MutateFrontendAction(const Options& options, int& mutation_id, int& file_id,
                       std::optional<protobufs::MutationInfo>& mutation_info,
                       std::set<std::string>& processed_files)
      : options_(&options),
        mutation_id_(&mutation_id),
        file_id_(&file_id),
        mutation_info_(&mutation_info),
        processed_files_(&processed_files) {}

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& compiler_instance,
      llvm::StringRef file) override;

  bool BeginInvocation(clang::CompilerInstance& compiler_instance) override {
    (void)compiler_instance;  // Unused.
    const bool input_exists = !getCurrentInput().isEmpty();
    (void)input_exists;  // Keep release-mode compilers happy.
    assert(input_exists && "No current file.");
    if (processed_files_->contains(getCurrentFile().str())) {
      llvm::errs() << "Warning: already processed " << getCurrentFile()
                   << "; skipping repeat occurrence.\n";
      return false;
    }
    processed_files_->insert(getCurrentFile().str());
    return true;
  }

 private:
  const Options* options_;
  int* mutation_id_;
  int* file_id_;
  std::optional<protobufs::MutationInfo>* mutation_info_;
  std::set<std::string>* processed_files_;
};

std::unique_ptr<clang::tooling::FrontendActionFactory>
NewMutateFrontendActionFactory(
    const Options& options, int& mutation_id, int& file_id,
    std::optional<protobufs::MutationInfo>& mutation_info) {
  class MutateFrontendActionFactory
      : public clang::tooling::FrontendActionFactory {
   public:
    MutateFrontendActionFactory(
        const Options& options, int& mutation_id, int& file_id,
        std::optional<protobufs::MutationInfo>& mutation_info)
        : options_(&options),
          mutation_id_(&mutation_id),
          file_id_(&file_id),
          mutation_info_(&mutation_info) {}

    std::unique_ptr<clang::FrontendAction> create() override {
      return std::make_unique<MutateFrontendAction>(*options_, *mutation_id_,
                                                    *file_id_, *mutation_info_,
                                                    processed_files_);
    }

   private:
    const Options* options_;
    int* mutation_id_;
    int* file_id_;
    std::optional<protobufs::MutationInfo>* mutation_info_;

    // Stores the ids of the files that have been processed so far, to avoid
    // processing a file multiple times.
    std::set<std::string> processed_files_;
  };

  return std::make_unique<MutateFrontendActionFactory>(options, mutation_id,
                                                       file_id, mutation_info);
}

std::unique_ptr<clang::ASTConsumer> MutateFrontendAction::CreateASTConsumer(
    clang::CompilerInstance& compiler_instance, llvm::StringRef file) {
  (void)file;  // Unused.
  return std::make_unique<MutateAstConsumer>(
      compiler_instance, *options_, *mutation_id_, *file_id_, *mutation_info_);
}

}  // namespace dredd
