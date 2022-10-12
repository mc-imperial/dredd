# This is a sample Python script.

# Press Shift+F10 to execute it or replace it with your code.
# Press Double Shift to search everywhere for classes, files, tool windows, actions, and settings.

import argparse
import functools
import json
import random

from pathlib import Path
from typing import Dict, List

class MutationTreeNode:
    def __init__(self, mutation_ids, children):
        self.children = children
        self.mutation_ids = mutation_ids



def get_mutation_ids_for_mutation_group(mutation_group):
    if "replaceExpr" in mutation_group:
        return [ instance["mutationId"] for instance in mutation_group["replaceExpr"]["instances"]]
    if "replaceBinaryOperator" in mutation_group:
        return [instance["mutationId"] for instance in mutation_group["replaceBinaryOperator"]["instances"]]
    if "replaceUnaryOperator" in mutation_group:
        return [instance["mutationId"] for instance in mutation_group["replaceUnaryOperator"]["instances"]]
    assert "removeStmt" in mutation_group
    return [mutation_group["removeStmt"]["mutationId"]]


def get_mutation_ids_for_json_node(node):
    assert "mutationGroups" in node
    return functools.reduce(lambda x, y: x + y, map(get_mutation_ids_for_mutation_group, node["mutationGroups"]), [])


class MutationTree:
    def __init__(self, json_data):

        def populate(json_node, node_id):
            children = []
            for child_json_node in json_node["children"]:
                child_node_id = self.num_nodes
                children.append(child_node_id)
                self.parent_map[child_node_id] = node_id
                self.num_nodes += 1
                populate(child_json_node, child_node_id)
            self.nodes[node_id] = MutationTreeNode(get_mutation_ids_for_json_node(json_node), children)
            self.num_mutations = max(self.num_mutations, functools.reduce(max, self.nodes[node_id].mutation_ids, 0))
            for mutation_id in self.nodes[node_id].mutation_ids:
                self.mutation_id_to_node_id[mutation_id] = node_id


        self.nodes = {}
        self.parent_map = {}
        self.mutation_id_to_node_id = {}
        self.num_mutations = 0
        self.num_nodes = 0

        for root_json_node in [file["mutationTreeRoot"] for file in json_data["infoForFiles"]]:
            root_node_id = self.num_nodes
            self.num_nodes += 1
            populate(root_json_node, root_node_id)

    def get_mutation_ids_for_subtree(self, node_id):
        assert 0 <= node_id < self.num_nodes
        return self.nodes[node_id].mutation_ids + functools.reduce(lambda x, y: x + y,
                                                                   map(lambda x: self.get_mutation_ids_for_subtree(x), self.nodes[node_id].children), [])

    def get_incompatible_mutation_ids(self, mutation_id):
        assert 0 <= mutation_id < self.num_mutations
        node_id = self.mutation_id_to_node_id[mutation_id]
        result = self.get_mutation_ids_for_subtree(node_id)
        while node_id in self.parent_map:
            node_id = self.parent_map[node_id]
            result += self.nodes[node_id].mutation_ids
        return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mutation_info_file", help="File containing information about mutations.", type=Path)
    parser.add_argument("generation_command", help="Command for input generation.", type=str)
    parser.add_argument("core_interestingness_test", help="Interestingness test to detect whether mutants are killed.", type=str)
    args = parser.parse_args()

    with open(args.mutation_info_file, 'r') as json_input:
        tree = MutationTree(json.load(json_input))

    available_mutations = list(range(0, tree.num_mutations))
    selected_mutations = []
    while len(selected_mutations) < 40000:
        index = random.randrange(0, len(available_mutations))
        mutation = available_mutations[index]
        selected_mutations.append(mutation)
        available_set = set(available_mutations)
        incompatible_set = set(tree.get_incompatible_mutation_ids(mutation))
        available_mutations = list(available_set.difference(incompatible_set))

    print(",".join([str(m) for m in selected_mutations]))

# Press the green button in the gutter to run the script.
if __name__ == '__main__':
    main()
