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

#ifndef LIBDREDD_MUTATE_AST_CONSUMER_H
#define LIBDREDD_MUTATE_AST_CONSUMER_H

#include <memory>
#include <string>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "libdredd/mutate_visitor.h"

namespace dredd {

class MutateAstConsumer : public clang::ASTConsumer {
 public:
  MutateAstConsumer(const clang::CompilerInstance& compiler_instance,
                    bool optimise_mutations, int& mutation_id)
      : compiler_instance_(compiler_instance),
        visitor_(std::make_unique<MutateVisitor>(compiler_instance)),
        mutation_id_(mutation_id),
        optimise_mutations_(optimise_mutations) {}

  void HandleTranslationUnit(clang::ASTContext& ast_context) override;

 private:
  [[nodiscard]] std::string GetDreddPreludeCpp(int initial_mutation_id) const;

  [[nodiscard]] std::string GetDreddPreludeC(int initial_mutation_id) const;

  const clang::CompilerInstance& compiler_instance_;
  std::unique_ptr<MutateVisitor> visitor_;
  clang::Rewriter rewriter_;

  // Counter used to give each mutation a unique id; shared among AST consumers
  // for different translation units.
  int& mutation_id_;
  // Used to disable dredd's mutations.
  const bool optimise_mutations_;
};

}  // namespace dredd

#endif  // LIBDREDD_MUTATE_AST_CONSUMER_H
