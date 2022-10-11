# This is a sample Python script.

# Press Shift+F10 to execute it or replace it with your code.
# Press Double Shift to search everywhere for classes, files, tool windows, actions, and settings.

import argparse
import functools
import json

from pathlib import Path
from typing import Dict, List

def get_max_mutation_id_for_mutation_group(mutation_group):
    if "replaceExpr" in mutation_group:
        return functools.reduce(max, [ instance["mutationId"] for instance in mutation_group["replaceExpr"]["instances"]])
    if "replaceBinaryOperator" in mutation_group:
        return functools.reduce(max,
                                [instance["mutationId"] for instance in mutation_group["replaceBinaryOperator"]["instances"]])
    if "replaceUnaryOperator" in mutation_group:
        return functools.reduce(max,
                                [instance["mutationId"] for instance in
                                 mutation_group["replaceUnaryOperator"]["instances"]])
    assert "removeStmt" in mutation_group
    return mutation_group["removeStmt"]["mutationId"]


def get_max_mutation_id_for_node(node):
    assert "children" in node
    result = functools.reduce(max, map(get_max_mutation_id_for_node, node["children"]), 0)
    assert "mutationGroups" in node
    result = max(result, functools.reduce(max, map(get_max_mutation_id_for_mutation_group, node["mutationGroups"]), 0))
    return result


def get_mutation_count(mutation_info: Dict):
    root_nodes: List = [ file["mutationTreeRoot"] for file in mutation_info["infoForFiles"]]
    return functools.reduce(max, map(get_max_mutation_id_for_node, root_nodes))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mutation_info_file", help="File containing information about mutations.", type=Path)
    parser.add_argument("generation_command", help="Command for input generation.", type=str)
    parser.add_argument("core_interestingness_test", help="Interestingness test to detect whether mutants are killed.", type=str)
    args = parser.parse_args()

    with open(args.mutation_info_file, 'r') as json_input:
        mutation_info = json.load(json_input)

    print(get_mutation_count(mutation_info))


# Press the green button in the gutter to run the script.
if __name__ == '__main__':
    main()
