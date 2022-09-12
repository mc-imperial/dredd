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

#ifndef LIBDREDD_MUTATION_H
#define LIBDREDD_MUTATION_H

#include <string>
#include <unordered_set>

#include "clang/AST/ASTContext.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"

namespace dredd {

// Interface that source code mutations should implement.
class Mutation {
 public:
  Mutation() = default;

  Mutation(const Mutation&) = delete;

  Mutation& operator=(const Mutation&) = delete;

  Mutation(Mutation&&) = delete;

  Mutation& operator=(Mutation&&) = delete;

  virtual ~Mutation();

  // The |first_mutation_id_in_file| argument can be subtracted from
  // |mutation_id| to turn it from a global mutation id (across all files) into
  // a local mutation id with respect to the particular source file being
  // mutated.
  //
  // The |dredd_declarations| argument provides a set of declarations that will
  // be added to the start of the source file being mutated. This allows
  // avoiding redundant repeat declarations.
  virtual void Apply(
      clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
      int first_mutation_id_in_file, int& mutation_id, bool optimise_mutations,
      clang::Rewriter& rewriter,
      std::unordered_set<std::string>& dredd_declarations) const = 0;
};

}  // namespace dredd

#endif  // LIBDREDD_MUTATION_H
