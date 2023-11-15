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

#ifndef LIBDREDD_NEW_MUTATE_FRONTEND_ACTION_FACTORY_H
#define LIBDREDD_NEW_MUTATE_FRONTEND_ACTION_FACTORY_H

#include <memory>

#include "clang/Tooling/Tooling.h"
#include "libdredd/protobufs/dredd_protobufs.h"

namespace dredd {

std::unique_ptr<clang::tooling::FrontendActionFactory>
NewMutateFrontendActionFactory(bool optimise_mutations,
                               bool semantics_preserving_mutation,
                               bool dump_asts, bool only_track_mutant_coverage,
                               int& mutation_id,
                               protobufs::MutationInfo& mutation_info);

}

#endif  // LIBDREDD_NEW_MUTATE_FRONTEND_ACTION_FACTORY_H
