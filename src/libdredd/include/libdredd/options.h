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
  class Optimisations {
   public:
    static Optimisations AllEnabled() { return Optimisations(true); }

    static Optimisations AllDisabled() { return Optimisations(false); }

    [[nodiscard]] bool GetDoNotRemoveSideEffectFreeExpressionStatements()
        const {
      return do_not_remove_side_effect_free_expression_statements_;
    }

    [[nodiscard]] bool GetDoNotRemoveCompoundStatements() const {
      return do_not_remove_compound_statements_;
    }

    [[nodiscard]] bool GetDoNotMutateCastsCleanupsAndParentheses() const {
      return do_not_mutate_casts_cleanups_and_parentheses_;
    }

    [[nodiscard]] bool GetLeverageConstantFolding() const {
      return leverage_constant_folding_;
    }

    [[nodiscard]] bool GetDoNotReplaceRelationalWithArgument() const {
      return do_not_replace_relational_with_argument_;
    }

    [[nodiscard]] bool GetAvoidRedundantOperatorMutationCombinations() const {
      return avoid_redundant_operator_mutation_combinations_;
    }

    [[nodiscard]] bool GetAvoidSelfInverseUnaryOperatorRemoval() const {
      return avoid_self_inverse_unary_operator_removal_;
    }

   private:
    explicit Optimisations(bool enabled)
        : do_not_remove_side_effect_free_expression_statements_(enabled),
          do_not_remove_compound_statements_(enabled),
          do_not_mutate_casts_cleanups_and_parentheses_(enabled),
          leverage_constant_folding_(enabled),
          do_not_replace_relational_with_argument_(enabled),
          avoid_redundant_operator_mutation_combinations_(enabled),
          avoid_self_inverse_unary_operator_removal_(enabled) {}

    bool do_not_remove_side_effect_free_expression_statements_;
    bool do_not_remove_compound_statements_;
    bool do_not_mutate_casts_cleanups_and_parentheses_;
    bool leverage_constant_folding_;
    bool do_not_replace_relational_with_argument_;
    bool avoid_redundant_operator_mutation_combinations_;
    bool avoid_self_inverse_unary_operator_removal_;
  };

  Options(const Optimisations& optimisations, bool dump_asts,
          bool only_track_mutant_coverage, bool show_ast_node_types)
      : optimisations_(optimisations),
        dump_asts_(dump_asts),
        only_track_mutant_coverage_(only_track_mutant_coverage),
        show_ast_node_types_(show_ast_node_types) {}

  Options() : Options(Optimisations::AllEnabled(), false, false, false) {}

  [[nodiscard]] Optimisations GetOptimisations() const {
    return optimisations_;
  }

  [[nodiscard]] bool GetOnlyTrackMutantCoverage() const {
    return only_track_mutant_coverage_;
  }

  [[nodiscard]] bool GetDumpAsts() const { return dump_asts_; }

  [[nodiscard]] bool GetShowAstNodeTypes() const {
    return show_ast_node_types_;
  }

 private:
  // Records which of Dredd's optimisations are enabled.
  Optimisations optimisations_;

  // True if and only if the AST being consumed should be dumped; useful for
  // debugging.
  bool dump_asts_;

  // True if and only if instrumentation should track whether mutants are
  // reached, rather than allowing mutants to be enabled.
  bool only_track_mutant_coverage_;

  // True if and only if a comment showing the type of each mutated AST node
  // should be emitted. This is useful for debugging.
  bool show_ast_node_types_;
};

}  // namespace dredd

#endif  // LIBDREDD_OPTIONS_H
