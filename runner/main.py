# This is a sample Python script.

# Press Shift+F10 to execute it or replace it with your code.
# Press Double Shift to search everywhere for classes, files, tool windows, actions, and settings.

import argparse
import functools
import jinja2
import json
import hashlib
import os
import random
import subprocess
import sys
import time

from pathlib import Path
from typing import Dict, List, Set

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

class GeneratedProgramStats:
    def __init__(self, compile_time: float, execution_time: float, expected_output: str,
                 executable_hash: str):
        self.compile_time = compile_time
        self.execution_time = execution_time
        self.expected_output = expected_output
        self.executable_hash = executable_hash


def hash_file(filename: str) -> str:
    md5_hash = hashlib.md5()
    md5_hash.update(open(filename, 'rb').read())
    return md5_hash.hexdigest()


def generate_a_program(csmith_root: Path,
                       compiler_executable: Path) -> GeneratedProgramStats:
    while True:
        # TODO: cleanup
        try:
            # Run csmith until it yields a program
            cmd: List[str] = [csmith_root / "build" / "src" / "csmith", "-o", "prog.c"]
            result = subprocess.run(cmd, timeout=10, capture_output=True)
            if result.returncode != 0:
                print("csmith failed")
                continue
        except subprocess.TimeoutExpired:
            print("csmith timed out")
            continue

        try:
            cmd: List[str] = [compiler_executable, "-O3", "-I", csmith_root / "runtime", "-I", csmith_root / "build" / "runtime", "prog.c", "-o", "prog"]
            compile_time_start: float = time.time()
            result = subprocess.run(cmd, timeout=10, capture_output=True)
            compile_time_end: float = time.time()
            if result.returncode != 0:
                print("compilation with non-mutated compiler failed")
                continue
        except subprocess.TimeoutExpired:
            print("compilation with non-mutated compiler timed out")
            continue

        try:
            cmd: List[str] = ["./prog"]
            execute_time_start: float = time.time()
            result = subprocess.run(cmd, timeout=10, capture_output=True)
            execute_time_end: float = time.time()
            if result.returncode != 0:
                print("execution of generated program compiled with non-mutated compiler failed")
                continue
        except subprocess.TimeoutExpired:
            print("execution of generated program compiled with non-mutated compiler timed out")
            continue

        return GeneratedProgramStats(compile_time=compile_time_end - compile_time_start,
                                     execution_time=execute_time_end - execute_time_start,
                                     expected_output=result.stdout.decode('utf-8'),
                                     executable_hash=hash_file('prog'))


def remove_mutants(unkilled_mutants: Set[int], to_remove: List[int]) -> None:
    for m in to_remove:
        unkilled_mutants.remove(m)


def generate_interestingness_test(selected_mutants: List[int]) -> None:
    assert len(selected_mutants) > 0
    test_script = "#!/bin/bash\n"
    test_script += "# Enabled mutants: " + ",".join([str(m) for m in selected_mutants]) + "\n"
    test_script += "# Check that program is free from certain kinds of compiler warning\n"
    test_script += "# Compile program with non-mutated compiler\n"
    test_script += "# Compile program with mutated compiler\n"
    test_script += "# Diff binaries\n"
    test_script += "# Check that results are different\n"
    test_script += "# Use sanitizers to check that program is good\n"
    print(test_script)
    assert False


def try_to_kill_mutants(csmith_root: Path,
                        compiler_executable: Path,
                        mutation_tree: MutationTree,
                        unkilled_mutants: Set[int],
                        untried_mutants: Set[int],
                        program_stats: GeneratedProgramStats,
                        num_simultaneous_mutations: int) -> bool:
    available_mutants: Set[int] = set(untried_mutants)
    selected_mutants: List[int] = []
    while len(available_mutants) > 0 and len(selected_mutants) < num_simultaneous_mutations:
        selected_mutant = list(available_mutants)[random.randrange(0, len(available_mutants))]
        selected_mutants.append(selected_mutant)
        available_mutants = available_mutants.difference(set(mutation_tree.get_incompatible_mutation_ids(selected_mutant)))

    try:
        cmd: List[str] = [compiler_executable, "-O3", "-I", csmith_root / "runtime", "-I",
                          csmith_root / "build" / "runtime", "prog.c", "-o", "prog_mutated"]
        mutated_environment: Dict[str, str] = os.environ.copy()
        mutated_environment['DREDD_ENABLED_MUTATION'] = ",".join([str(m) for m in selected_mutants])
        result = subprocess.run(cmd, timeout=max(5.0, 5.0*program_stats.compile_time), capture_output=True,
                                env=mutated_environment)
    except subprocess.TimeoutExpired:
        print("WEAK KILL: Compilation with mutated compiler timed out.")
        remove_mutants(unkilled_mutants, selected_mutants)
        return True

    if result.returncode != 0:
        print("WEAK KILL: Compilation with mutated compiler failed.")
        remove_mutants(unkilled_mutants, selected_mutants)
        return True

    print("Compilation with mutated compiler succeeded")
    if hash_file('prog_mutated') == program_stats.executable_hash:
        print("Binaries are the same - not interesting")
        return False

    print("Different binaries!")
    try:
        cmd: List[str] = ["./prog_mutated"]
        result = subprocess.run(cmd, timeout=max(5.0, 10.0*program_stats.execution_time), capture_output=True)
        if result.returncode != 0:
            print("STRONG KILL: Execution of program compiled with mutated compiler failed.")
            remove_mutants(unkilled_mutants, selected_mutants)
            return True
        if result.stdout.decode('utf-8') != program_stats.expected_output:
            print("VERY STRONG KILL: Execution results from program compiled with mutated compiler are different!")
            generate_interestingness_test(selected_mutants)
            remove_mutants(unkilled_mutants, selected_mutants)
            return True

    except subprocess.TimeoutExpired:
        print("STRONG KILL: Execution of program compiled with mutated compiler timed out.")
        remove_mutants(unkilled_mutants, selected_mutants)
        return True

    print("Same execution results - not interesting")
    return False


def main():

    parser = argparse.ArgumentParser()
    parser.add_argument("mutation_info_file", help="File containing information about mutations.", type=Path)
    parser.add_argument("compiler_executable", help="Path to the executable for the Dredd-mutated compiler.", type=Path)
    parser.add_argument("csmith_root", help="Path to a checkout of Csmith, assuming that it has been built under 'build' beneath this directory.", type=Path)
    parser.add_argument("--max_consecutive_failed_attempts_per_program", default=10, help="The number of times a given generated program will be compiled and executed in a row with no mutation being killed.", type=int)
    parser.add_argument("--max_attempts_per_program", default=100, help="The number of times a given program will be compiled and executed to look for kills (excluding work done investigating kills) before moving on to the next program.", type=int)
    parser.add_argument("--num_simultaneous_mutations", default=64, help="The number of mutations that will be enabled simultaneously when looking for kills.", type=int)
    args = parser.parse_args()

    print("Building the mutation tree...")
    with open(args.mutation_info_file, 'r') as json_input:
        mutation_tree = MutationTree(json.load(json_input))
    print("Built!")
    unkilled_mutants: Set[int] = set(range(0, mutation_tree.num_mutations))

    while True:
        print("Generating a program...")
        program_stats: GeneratedProgramStats = generate_a_program(csmith_root = args.csmith_root,
                           compiler_executable = args.compiler_executable)
        print("Generated!")
        untried_mutants: Set[int] = unkilled_mutants
        num_attempts_for_current_program: int = 0
        num_consecutive_failed_attempts_for_current_program: int = 0
        while num_attempts_for_current_program < args.max_attempts_per_program and num_consecutive_failed_attempts_for_current_program < args.max_consecutive_failed_attempts_per_program:
            success: bool = try_to_kill_mutants(csmith_root = args.csmith_root,
                                                compiler_executable = args.compiler_executable,
                                                mutation_tree = mutation_tree,
                                                unkilled_mutants = unkilled_mutants,
                                                untried_mutants = untried_mutants,
                                                program_stats = program_stats,
                                                num_simultaneous_mutations = args.num_simultaneous_mutations)
            sys.stdout.flush()
            if not success:
                num_consecutive_failed_attempts_for_current_program += 1
            else:
                num_consecutive_failed_attempts_for_current_program = 0
            num_attempts_for_current_program += 1

# Press the green button in the gutter to run the script.
if __name__ == '__main__':
    main()
