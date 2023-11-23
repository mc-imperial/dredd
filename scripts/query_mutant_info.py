import argparse
import json
import re
import sys

from pathlib import Path
from typing import Dict


def build_mapping_for_node(mutation_tree_node: Dict, file_info: Dict, result: Dict[int, Dict]) -> None:
    for mutation_group in mutation_tree_node["mutationGroups"]:
        assert len(mutation_group) == 1
        key: str = next(iter(mutation_group))
        if key in ["replaceExpr", "replaceUnaryOperator", "replaceBinaryOperator"]:
            for instance in mutation_group[key]["instances"]:
                result[instance["mutationId"]] = {
                    "kind": key,
                    "fileInfo": file_info,
                    "mutationGroup": mutation_group,
                    "instance": instance
                }
        else:
            assert key == "removeStmt"
            result[mutation_group[key]["mutationId"]] = {
                "kind": key,
                "fileInfo": file_info,
                "mutationGroup": mutation_group
            }
    for child in mutation_tree_node["children"]:
        build_mapping_for_node(child, file_info, result)


def build_mutant_to_node_mapping(json_info: Dict) -> Dict[int, Dict]:
    result: Dict[int, Dict] = {}
    for file_info in json_info["infoForFiles"]:
        build_mapping_for_node(file_info["mutationTreeRoot"], file_info, result)
    return result


def parenthesise_if_needed(expr_text: str) -> str:
    # Do not parenthesise if it is clearly unnecessary. This regex captures a bunch
    # of obvious cases, such as identifiers and numeric literals.
    if re.match(r'^[a-zA-Z0-9_\.\s]+$', expr_text):
        return expr_text
    return f"({expr_text})"


# Returns:
# (range is on single line?, range description, blank prefix, text for range)
def get_text_for_source_range(start_location: Dict[str, int],
                              end_location: Dict[str, int],
                              filename: str) -> (bool, str, str, str):
    start_line = start_location['line']
    start_column = start_location['column']
    end_line = end_location['line']
    end_column = end_location['column']
    range_description = f"{filename}, {start_line}:{start_column}--{end_line}:{end_column}"

    lines = open(filename, 'r').readlines()
    if start_line == end_line:
        return True, range_description, "", lines[start_line - 1][start_column - 1:end_column - 1]
    blank_prefix: str = " " * (start_column - 1)
    source_range_text: str = lines[start_line - 1][start_column - 1:]
    for i in range(start_line + 1, end_line - 1):
        source_range_text += lines[i]
    source_range_text += lines[end_line - 1][0:end_column - 1]
    return False, range_description, blank_prefix, source_range_text


def binary_operator_replacement_text(single_line: bool,
                                     lhs_blank_prefix: str,
                                     lhs_text: str,
                                     rhs_blank_prefix: str,
                                     rhs_text: str,
                                     action: str) -> str:
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
    result = f'{lhs_blank_prefix}{parenthesise_if_needed(lhs_text)}'
    if single_line:
        result += ' '
    else:
        result += '\n'
    result += action_to_operator[action]
    if single_line:
        result += ' '
    else:
        result += '\n'
    result += f'{rhs_blank_prefix}{parenthesise_if_needed(rhs_text)}'
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mutation_info_file",
                        help="File containing information about mutations, generated when Dredd was used to mutate the source code.",
                        type=Path)
    parser.add_argument("--largest-mutant-id",
                        help="Show the largest id among all mutants that are present. Mutants are normally numbered contiguously starting from 0, so this is related to the total number of mutants.",
                        action='store_true')
    parser.add_argument("--show-info-for-mutant",
                        help="Show information about a given mutant",
                        type=int)

    args=parser.parse_args()
    with open(args.mutation_info_file, 'r') as json_input:
        json_info = json.load(json_input)
    mapping = build_mutant_to_node_mapping(json_info)

    if args.largest_mutant_id:
        print(max(list(mapping)))
        return 0

    if args.show_info_for_mutant is not None:
        mutant_id: int = args.show_info_for_mutant
        if mutant_id not in mapping:
            print(f"Unknown mutant id: {mutant_id}")
            return 1
        mutant_info = mapping[mutant_id]
        filename = mutant_info['fileInfo']['filename']
        if mutant_info["kind"] == "replaceExpr":
            _, range_description, blank_prefix, expr_text = get_text_for_source_range(
                start_location=mutant_info['mutationGroup']['replaceExpr']['start'],
                end_location=mutant_info['mutationGroup']['replaceExpr']['end'],
                filename=filename)
            print(f"Replace expression at {range_description}\n")
            print(f"Original expression:\n")
            print(f"{blank_prefix}{expr_text}\n")
            action = mutant_info['instance']['action']

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
        elif mutant_info["kind"] == "replaceUnaryOperator":
            single_line, expr_range_description, expr_blank_prefix, expr_text = get_text_for_source_range(
                start_location=mutant_info['mutationGroup']['replaceUnaryOperator']['exprStart'],
                end_location=mutant_info['mutationGroup']['replaceUnaryOperator']['exprEnd'],
                filename=filename)
            _, _, operand_blank_prefix, operand_text = get_text_for_source_range(
                start_location=mutant_info['mutationGroup']['replaceUnaryOperator']['operandStart'],
                end_location=mutant_info['mutationGroup']['replaceUnaryOperator']['operandEnd'],
                filename=filename)
            print(f"Replace unary operator expression at {expr_range_description}\n")
            print(f"Original unary operator expression:\n")
            print(f"{expr_blank_prefix}{expr_text}\n")
            action = mutant_info['instance']['action']
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
                replacement_expr_text = operand_blank_prefix + pre_replacements[action] + " " + parenthesise_if_needed(operand_text)
            else:
                assert action in post_replacements
                replacement_expr_text = operand_blank_prefix + parenthesise_if_needed(operand_text) + " " + post_replacements[action]
            print(f"Replacement expression:\n")
            print(f"{replacement_expr_text}\n")
            return 0
        elif mutant_info["kind"] == "replaceBinaryOperator":
            single_line, expr_range_description, expr_blank_prefix, expr_text = get_text_for_source_range(
                start_location=mutant_info['mutationGroup']['replaceBinaryOperator']['exprStart'],
                end_location=mutant_info['mutationGroup']['replaceBinaryOperator']['exprEnd'],
                filename=filename)
            _, _, lhs_blank_prefix, lhs_text = get_text_for_source_range(
                start_location=mutant_info['mutationGroup']['replaceBinaryOperator']['lhsStart'],
                end_location=mutant_info['mutationGroup']['replaceBinaryOperator']['lhsEnd'],
                filename=filename)
            _, _, rhs_blank_prefix, rhs_text = get_text_for_source_range(
                start_location=mutant_info['mutationGroup']['replaceBinaryOperator']['rhsStart'],
                end_location=mutant_info['mutationGroup']['replaceBinaryOperator']['rhsEnd'],
                filename=filename)
            print(f"Replace binary operator expression at {expr_range_description}\n")
            print(f"Original binary operator expression:\n")
            print(f"{expr_blank_prefix}{expr_text}\n")
            action = mutant_info['instance']['action']
            if action == 'ReplaceWithLHS':
                replacement_expr_text = lhs_blank_prefix + lhs_text
            elif action == 'ReplaceWithRHS':
                replacement_expr_text = rhs_blank_prefix + rhs_text
            else:
                replacement_expr_text = binary_operator_replacement_text(
                    single_line=single_line,
                    lhs_blank_prefix=lhs_blank_prefix,
                    lhs_text=lhs_text,
                    rhs_blank_prefix=rhs_blank_prefix,
                    rhs_text=rhs_text,
                    action=action)
            print(f"Replacement expression:\n")
            print(f"{replacement_expr_text}\n")
            return 0
        else:
            assert mutant_info["kind"] == "removeStmt"
            _, range_description, blank_prefix, stmt_text = get_text_for_source_range(
                start_location=mutant_info['mutationGroup']['removeStmt']['start'],
                end_location = mutant_info['mutationGroup']['removeStmt']['end'],
                filename=filename)
            print(f"Remove statement at {range_description}\n")
            print(f"This is the removed statement:\n")
            print(blank_prefix + stmt_text)
            return 0

    print("No action specified.")
    return 1


if __name__ == '__main__':
    sys.exit(main())
