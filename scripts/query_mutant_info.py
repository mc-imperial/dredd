#!/usr/bin/env python3

# Copyright 2023 The Dredd Project Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import json
import os
import re
import sys

from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Optional


@dataclass
class MutantInfo:
    kind: str
    file_info: Dict
    mutation_group: Dict
    instance: Optional[Dict]


def build_mapping_for_node(mutation_tree_node: Dict, file_info: Dict, result: Dict[int, MutantInfo]) -> None:
    for mutation_group in mutation_tree_node["mutationGroups"]:
        assert len(mutation_group) == 1
        key: str = next(iter(mutation_group))
        if key in ["replaceExpr", "replaceUnaryOperator", "replaceBinaryOperator"]:
            for instance in mutation_group[key]["instances"]:
                result[instance["mutationId"]] = MutantInfo(key, file_info, mutation_group, instance)
        else:
            assert key == "removeStmt"
            result[mutation_group[key]["mutationId"]] = MutantInfo(key, file_info, mutation_group, None)
    for child in mutation_tree_node["children"]:
        build_mapping_for_node(child, file_info, result)


def build_mutant_to_node_mapping(json_info: Dict) -> Dict[int, MutantInfo]:
    result: Dict[int, MutantInfo] = {}
    for file_info in json_info["infoForFiles"]:
        build_mapping_for_node(file_info["mutationTreeRoot"], file_info, result)
    return result


def parenthesise_if_needed(expr_text: str) -> str:
    # Do not parenthesise if it is clearly unnecessary. This regex captures a bunch
    # of obvious cases, such as identifiers and numeric literals.
    if re.match(r'^[a-zA-Z0-9_\\.\s]+$', expr_text):
        return expr_text
    return f"({expr_text})"


# Returns:
# (range is on single line?, range description, blank prefix, text for range)
def get_text_for_source_range(start_location: Dict[str, int],
                              end_location: Dict[str, int],
                              original_source_code_filename: str,
                              filename_without_prefix: str) -> (bool, str, str, str):
    start_line = start_location['line']
    start_column = start_location['column']
    end_line = end_location['line']
    end_column = end_location['column']
    range_description = f"{filename_without_prefix}, {start_line}:{start_column}--{end_line}:{end_column}"

    lines = open(original_source_code_filename, 'r').readlines()
    if start_line == end_line:
        return True, range_description, "", lines[start_line - 1][start_column - 1:end_column - 1]
    blank_prefix: str = " " * (start_column - 1)
    source_range_text: str = lines[start_line - 1][start_column - 1:]
    for i in range(start_line + 1, end_line - 1):
        source_range_text += lines[i]
    source_range_text += lines[end_line - 1][0:end_column - 1]
    return False, range_description, blank_prefix, source_range_text


def show_info_for_replace_expr(mutant_info: MutantInfo,
                               original_source_code_filename: str,
                               filename_without_prefix: str) -> int:
    _, range_description, blank_prefix, expr_text = get_text_for_source_range(
        start_location=mutant_info.mutation_group['replaceExpr']['start'],
        end_location=mutant_info.mutation_group['replaceExpr']['end'],
        original_source_code_filename=original_source_code_filename,
        filename_without_prefix=filename_without_prefix)
    print(f"Replace expression at {range_description}\n")
    print(f"Original expression:\n")
    print(f"{blank_prefix}{expr_text}\n")
    action = mutant_info.instance['action']

    replacements: Dict[str, str] = {
        "ReplaceWithZeroFloat": "0.0",
        "ReplaceWithOneFloat": "1.0",
        "ReplaceWithMinusOneFloat": "-1.0",
        "ReplaceWithZeroInt": "0",
        "ReplaceWithOneInt": "1",
        "ReplaceWithMinusOneInt": "-1",
        "ReplaceWithTrue": "true",
        "ReplaceWithFalse": "false",
    }
    insertions: Dict[str, str] = {
        "InsertPreInc": "++",
        "InsertPreDec": "--",
        "InsertLNot": "!",
        "InsertNot": "~",
        "InsertMinus": "-",
    }
    if action in replacements:
        replacement_expr_text = replacements[action]
    else:
        assert action in insertions
        replacement_expr_text = blank_prefix + insertions[action] + " " + parenthesise_if_needed(expr_text)
    print(f"Replacement expression:\n")
    print(f"{replacement_expr_text}\n")
    return 0


def show_info_for_replace_unary_operator(mutant_info: MutantInfo,
                                         original_source_code_filename: str,
                                         filename_without_prefix: str) -> int:
    replace_unary_operator = mutant_info.mutation_group['replaceUnaryOperator']
    single_line, expr_range_description, expr_blank_prefix, expr_text = get_text_for_source_range(
        start_location=replace_unary_operator['exprStart'],
        end_location=replace_unary_operator['exprEnd'],
        original_source_code_filename=original_source_code_filename,
        filename_without_prefix=filename_without_prefix)
    _, _, operand_blank_prefix, operand_text = get_text_for_source_range(
        start_location=replace_unary_operator['operandStart'],
        end_location=replace_unary_operator['operandEnd'],
        original_source_code_filename=original_source_code_filename,
        filename_without_prefix=filename_without_prefix)
    print(f"Replace unary operator expression at {expr_range_description}\n")
    print(f"Original unary operator expression:\n")
    print(f"{expr_blank_prefix}{expr_text}\n")
    action = mutant_info.instance['action']
    pre_replacements: Dict[str, str] = {
        "ReplaceWithMinus": "-",
        "ReplaceWithNot": "~",
        "ReplaceWithPreDec": "--",
        "ReplaceWithPreInc": "++",
        "ReplaceWithLNot": "!",
    }
    post_replacements: Dict[str, str] = {
        "ReplaceWithPostDec": "--",
        "ReplaceWithPostInc": "++",
    }
    if action == 'ReplaceWithOperand':
        replacement_expr_text = operand_blank_prefix + operand_text
    elif action in pre_replacements:
        replacement_expr_text = operand_blank_prefix + pre_replacements[action] + " " + parenthesise_if_needed(
            operand_text)
    else:
        assert action in post_replacements
        replacement_expr_text = (operand_blank_prefix + parenthesise_if_needed(operand_text) + " "
                                 + post_replacements[action])
    print(f"Replacement expression:\n")
    print(f"{replacement_expr_text}\n")
    return 0


def show_info_for_replace_binary_operator(mutant_info: MutantInfo,
                                          original_source_code_filename: str,
                                          filename_without_prefix: str):
    replace_binary_operator = mutant_info.mutation_group['replaceBinaryOperator']
    single_line, expr_range_description, expr_blank_prefix, expr_text = get_text_for_source_range(
        start_location=replace_binary_operator['exprStart'],
        end_location=replace_binary_operator['exprEnd'],
        original_source_code_filename=original_source_code_filename,
        filename_without_prefix=filename_without_prefix)
    _, _, lhs_blank_prefix, lhs_text = get_text_for_source_range(
        start_location=replace_binary_operator['lhsStart'],
        end_location=replace_binary_operator['lhsEnd'],
        original_source_code_filename=original_source_code_filename,
        filename_without_prefix=filename_without_prefix)
    _, _, rhs_blank_prefix, rhs_text = get_text_for_source_range(
        start_location=replace_binary_operator['rhsStart'],
        end_location=replace_binary_operator['rhsEnd'],
        original_source_code_filename=original_source_code_filename,
        filename_without_prefix=filename_without_prefix)
    print(f"Replace binary operator expression at {expr_range_description}\n")
    print(f"Original binary operator expression:\n")
    print(f"{expr_blank_prefix}{expr_text}\n")
    action = mutant_info.instance['action']
    if action == 'ReplaceWithLHS':
        replacement_expr_text = lhs_blank_prefix + lhs_text
    elif action == 'ReplaceWithRHS':
        replacement_expr_text = rhs_blank_prefix + rhs_text
    else:
        action_to_operator: Dict[str, str] = {
            "ReplaceWithAdd": "+",
            "ReplaceWithDiv": "/",
            "ReplaceWithMul": "*",
            "ReplaceWithRem": "%",
            "ReplaceWithSub": "-",
            "ReplaceWithAddAssign": "+=",
            "ReplaceWithAndAssign": "&=",
            "ReplaceWithAssign": "=",
            "ReplaceWithDivAssign": "/=",
            "ReplaceWithMulAssign": "*=",
            "ReplaceWithOrAssign": "|=",
            "ReplaceWithRemAssign": "%=",
            "ReplaceWithShlAssign": "<<=",
            "ReplaceWithShrAssign": ">>=",
            "ReplaceWithSubAssign": "-=",
            "ReplaceWithXorAssign": "^=",
            "ReplaceWithAnd": "&",
            "ReplaceWithOr": "|",
            "ReplaceWithXor": "^",
            "ReplaceWithLAnd": "&&",
            "ReplaceWithLOr": "||",
            "ReplaceWithEQ": "==",
            "ReplaceWithGE": ">=",
            "ReplaceWithGT": ">",
            "ReplaceWithLE": "<=",
            "ReplaceWithLT": "<",
            "ReplaceWithNE": "!=",
            "ReplaceWithShl": "<<",
            "ReplaceWithShr": ">>",
        }
        replacement_expr_text = f'{lhs_blank_prefix}{parenthesise_if_needed(lhs_text)}'
        if single_line:
            replacement_expr_text += ' '
        else:
            replacement_expr_text += '\n'
        replacement_expr_text += action_to_operator[action]
        if single_line:
            replacement_expr_text += ' '
        else:
            replacement_expr_text += '\n'
        replacement_expr_text += f'{rhs_blank_prefix}{parenthesise_if_needed(rhs_text)}'
    print(f"Replacement expression:\n")
    print(f"{replacement_expr_text}\n")
    return 0


def show_info_for_remove_stmt(mutant_info: MutantInfo,
                              original_source_code_filename: str,
                              filename_without_prefix: str) -> int:
    remove_stmt = mutant_info.mutation_group['removeStmt']
    _, range_description, blank_prefix, stmt_text = get_text_for_source_range(
        start_location=remove_stmt['start'],
        end_location=remove_stmt['end'],
        original_source_code_filename=original_source_code_filename,
        filename_without_prefix=filename_without_prefix)
    print(f"Remove statement at {range_description}\n")
    print(f"This is the removed statement:\n")
    print(blank_prefix + stmt_text)
    return 0


def show_info_for_mutant(args, mapping: Dict[int, MutantInfo]) -> int:
    if args.path_prefix_replacement is None:
        print("Missing: path to the root of the mutated code, and path to the root of the non-mutated code. From "
              "these prefixes onwards, the source trees should have the same content.")
        return 1

    mutant_id: int = args.show_info_for_mutant
    if mutant_id not in mapping:
        print(f"Unknown mutant id: {mutant_id}")
        return 1
    mutant_info: MutantInfo = mapping[mutant_id]
    mutated_source_code_filename = mutant_info.file_info['filename']
    filename_without_prefix = mutated_source_code_filename[len(str(args.path_prefix_replacement[0])):]
    if filename_without_prefix.startswith(os.sep):
        filename_without_prefix = filename_without_prefix[1:]
    original_source_code_filename = str(args.path_prefix_replacement[1]) + os.sep + filename_without_prefix
    if mutant_info.kind == "replaceExpr":
        return show_info_for_replace_expr(mutant_info=mutant_info,
                                          original_source_code_filename=original_source_code_filename,
                                          filename_without_prefix=filename_without_prefix)
    elif mutant_info.kind == "replaceUnaryOperator":
        return show_info_for_replace_unary_operator(mutant_info=mutant_info,
                                                    original_source_code_filename=original_source_code_filename,
                                                    filename_without_prefix=filename_without_prefix)
    elif mutant_info.kind == "replaceBinaryOperator":
        return show_info_for_replace_binary_operator(mutant_info=mutant_info,
                                                     original_source_code_filename=original_source_code_filename,
                                                     filename_without_prefix=filename_without_prefix)
    else:
        assert mutant_info.kind == "removeStmt"
        return show_info_for_remove_stmt(mutant_info=mutant_info,
                                         original_source_code_filename=original_source_code_filename,
                                         filename_without_prefix=filename_without_prefix)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mutation_info_file",
                        help="File containing information about mutations, generated when Dredd was used to mutate "
                             "the source code. The source files referred to in this file come from the version of the "
                             "code base that was mutated.",
                        type=Path)
    parser.add_argument("--path-prefix-replacement",
                        help="When displaying information about the effect of mutants, we want to refer to the non-"
                             "mutated source code. If the mutated code base is at /path/to/somewhere/mutated, and the "
                             "non-mutated codebase is at /path/to/elsewhere/nonmutated, then this pair of prefixes "
                             "should be passed. They allow files in the non-mutated code base to be opened when "
                             "looking for source code contents to display.",
                        nargs=2,
                        metavar=('path_to_mutated_code', 'path_to_original_code'),
                        type=Path)
    parser.add_argument("--largest-mutant-id",
                        help="Show the largest id among all mutants that are present. Mutants are normally numbered "
                             "contiguously starting from 0, so this is related to the total number of mutants.",
                        action='store_true')
    parser.add_argument("--show-info-for-mutant",
                        help="Show information about a given mutant",
                        type=int)

    args = parser.parse_args()
    with open(args.mutation_info_file, 'r') as json_input:
        json_info = json.load(json_input)
    mapping: Dict[int, MutantInfo] = build_mutant_to_node_mapping(json_info)

    if args.largest_mutant_id:
        mutant_ids: List[int] = list(mapping.keys())
        print(0 if not mutant_ids else max(mutant_ids))
        return 0

    if args.show_info_for_mutant is not None:
        return show_info_for_mutant(args, mapping)

    print("No action specified.")
    return 1


if __name__ == '__main__':
    sys.exit(main())
