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
    explicit Optimisations(
        bool do_not_remove_side_effect_free_expression_statements,
        bool do_not_remove_compound_statements,
        bool do_not_mutate_casts_cleanups_and_parentheses,
        bool leverage_constant_folding,
        bool do_not_replace_relational_with_argument,
        bool avoid_redundant_operator_mutation_combinations,
        bool avoid_self_inverse_unary_operator_removal,
        bool do_not_mutate_sizeof_and_alignof)
        : do_not_remove_side_effect_free_expression_statements_(
              do_not_remove_side_effect_free_expression_statements),
          do_not_remove_compound_statements_(do_not_remove_compound_statements),
          do_not_mutate_casts_cleanups_and_parentheses_(
              do_not_mutate_casts_cleanups_and_parentheses),
          leverage_constant_folding_(leverage_constant_folding),
          do_not_replace_relational_with_argument_(
              do_not_replace_relational_with_argument),
          avoid_redundant_operator_mutation_combinations_(
              avoid_redundant_operator_mutation_combinations),
          avoid_self_inverse_unary_operator_removal_(
              avoid_self_inverse_unary_operator_removal),
          do_not_mutate_sizeof_and_alignof_(do_not_mutate_sizeof_and_alignof) {}

    static Optimisations AllEnabled() {
      return Optimisations(true, true, true, true, true, true, true, true);
    }

    static Optimisations AllDisabled() {
      return Optimisations(false, false, false, false, false, false, false,
                           false);
    }

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

    [[nodiscard]] bool DoNotMutateSizeofAndAlignof() const {
      return do_not_mutate_sizeof_and_alignof_;
    }

   private:
    bool do_not_remove_side_effect_free_expression_statements_;
    bool do_not_remove_compound_statements_;
    bool do_not_mutate_casts_cleanups_and_parentheses_;
    bool leverage_constant_folding_;
    bool do_not_replace_relational_with_argument_;
    bool avoid_redundant_operator_mutation_combinations_;
    bool avoid_self_inverse_unary_operator_removal_;
    bool do_not_mutate_sizeof_and_alignof_;
  };

  enum class EnablednessCheckingMode {
    // The default, optimised mode, where the contents of the environment
    // variable are read once per file, and each enabledness check first checks
    // whether some mutation is enabled for the file before checking each
    // potential mutant case.
    STANDARD,
    // The contents of the environment variable is read once per file, but
    // thereafter the pre-check for per-file enabledness is omitted. This is
    // included merely for comparison purposes against the standard mode; it is
    // expected to be less efficient.
    NO_SOME_ENABLED_CHECK,
    // The environment variable is read from every time an enabledness check is
    // made. This is likely to be very efficient and is provided merely for
    // comparison purposes.
    ALWAYS_READ_ENVIRONMENT_VARIABLE,
  };

  Options(const Optimisations& optimisations, bool dump_asts,
          bool only_track_mutant_coverage, bool show_ast_node_types,
          EnablednessCheckingMode enabledness_checking_mode)
      : optimisations_(optimisations),
        dump_asts_(dump_asts),
        only_track_mutant_coverage_(only_track_mutant_coverage),
        show_ast_node_types_(show_ast_node_types),
        enabledness_checking_mode_(enabledness_checking_mode) {}

  Options()
      : Options(Optimisations::AllEnabled(), false, false, false,
                EnablednessCheckingMode::STANDARD) {}

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

  [[nodiscard]] EnablednessCheckingMode GetEnablednessCheckingMode() const {
    return enabledness_checking_mode_;
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

  // Controls the mode used for checking mutant enabledness.
  EnablednessCheckingMode enabledness_checking_mode_;
};

}  // namespace dredd

#endif  // LIBDREDD_OPTIONS_H
