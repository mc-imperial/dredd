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

#include "libdredd/mutation_replace_expr.h"

namespace dredd {
dredd::MutationReplaceExpr::MutationReplaceExpr(const clang::Expr& expr)
    : expr_(expr) {}

void MutationReplaceExpr::Apply(
    clang::ASTContext& ast_context, const clang::Preprocessor& preprocessor,
    int first_mutation_id_in_file, int& mutation_id, clang::Rewriter& rewriter,
    std::unordered_set<std::string>& dredd_declarations) const {}
}  // namespace dredd
