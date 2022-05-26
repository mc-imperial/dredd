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

#ifndef DREDD_MUTATE_AST_CONSUMER_H
#define DREDD_MUTATE_AST_CONSUMER_H

#include <cstddef>
#include <memory>
#include <random>
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
                    size_t num_mutations, const std::string& output_file,
                    std::mt19937& generator)
      : compiler_instance_(compiler_instance),
        num_mutations_(num_mutations),
        output_file_(output_file),
        generator_(generator),
        visitor_(std::make_unique<MutateVisitor>(compiler_instance)) {}

  void HandleTranslationUnit(clang::ASTContext& context) override;

 private:
  const clang::CompilerInstance& compiler_instance_;
  const size_t num_mutations_;
  const std::string& output_file_;
  std::mt19937& generator_;
  std::unique_ptr<MutateVisitor> visitor_;
  clang::Rewriter rewriter_;
};

}  // namespace dredd

#endif  // DREDD_MUTATE_AST_CONSUMER_H
