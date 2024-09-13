// Copyright 2024 The Dredd Project Authors
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

#ifndef LIBDREDD_OPTIONS_H
#define LIBDREDD_OPTIONS_H

namespace dredd {

class Options {
 public:
  Options(bool optimise_mutations, bool dump_asts,
          bool only_track_mutant_coverage, bool show_ast_node_types)
      : optimise_mutations_(optimise_mutations),
        dump_asts_(dump_asts),
        only_track_mutant_coverage_(only_track_mutant_coverage),
        show_ast_node_types_(show_ast_node_types) {}

  Options() : Options(true, false, false, false) {}

  [[nodiscard]] bool GetOptimiseMutations() const {
    return optimise_mutations_;
  }

  [[nodiscard]] bool GetOnlyTrackMutantCoverage() const {
    return only_track_mutant_coverage_;
  }

  [[nodiscard]] bool GetDumpAsts() const { return dump_asts_; }

  [[nodiscard]] bool GetShowAstNodeTypes() const {
    return show_ast_node_types_;
  }

 private:
  // True if and only if Dredd's optimisations are enabled.
  const bool optimise_mutations_;

  // True if and only if the AST being consumed should be dumped; useful for
  // debugging.
  const bool dump_asts_;

  // True if and only if instrumentation should track whether mutants are
  // reached, rather than allowing mutants to be enabled.
  const bool only_track_mutant_coverage_;

  // True if and only if a comment showing the type of each mutated AST node
  // should be emitted. This is useful for debugging.
  const bool show_ast_node_types_;
};

}  // namespace dredd

#endif  // LIBDREDD_OPTIONS_H
