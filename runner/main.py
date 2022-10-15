# This is a sample Python script.

# Press Shift+F10 to execute it or replace it with your code.
# Press Double Shift to search everywhere for classes, files, tool windows, actions, and settings.

import argparse
import functools
import shutil

import jinja2
import json
import hashlib
import os
import random
import stat
import subprocess
import sys
import time

import prepare_csmith_program

from enum import auto, Enum
from pathlib import Path
from typing import Dict, List, Set


class MutationTreeNode:
    def __init__(self, mutation_ids, children):
        self.children = children
        self.mutation_ids = mutation_ids


def get_mutation_ids_for_mutation_group(mutation_group):
    if "replaceExpr" in mutation_group:
        return [instance["mutationId"] for instance in mutation_group["replaceExpr"]["instances"]]
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
                                                                   map(lambda x: self.get_mutation_ids_for_subtree(x),
                                                                       self.nodes[node_id].children), [])

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
            cmd: List[str] = [csmith_root / "build" / "src" / "csmith", "-o", "__prog.c"]
            result = subprocess.run(cmd, timeout=10, capture_output=True)
            if result.returncode != 0:
                print("csmith failed")
                continue
        except subprocess.TimeoutExpired:
            print("csmith timed out")
            continue

        # Inline some immediate header files into the Csmith-generated program
        prepare_csmith_program.prepare(Path("__prog.c"), Path("__prog.c"), csmith_root)

        try:
            cmd: List[str] = [compiler_executable, "-O3", "-I", csmith_root / "runtime", "-I", csmith_root / "build"
                              / "runtime", "__prog.c", "-o", "prog"]
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


def generate_interestingness_test(csmith_root: Path,
                                  compiler_executable: Path,
                                  mutant: int) -> None:
    template_loader = jinja2.FileSystemLoader(searchpath="./")
    template_env = jinja2.Environment(loader=template_loader)
    template = template_env.get_template("interesting.py.template")
    open('__interesting.py', 'w').write(template.render(csmith_root=csmith_root,
                                                        compiler_executable=compiler_executable,
                                                        mutation_ids=str(mutant)))
    # Make the interestingness test executable.
    st = os.stat('__interesting.py')
    os.chmod('__interesting.py', st.st_mode | stat.S_IEXEC)


def reduce_very_strong_kill(csmith_root: Path,
                            compiler_executable: Path,
                            mutant: int) -> bool:
    generate_interestingness_test(csmith_root=csmith_root,
                                  compiler_executable=compiler_executable,
                                  mutant=mutant)
    creduce_environment = os.environ.copy()
    creduce_environment['CREDUCE_INCLUDE_PATH'] =\
        str(csmith_root / 'runtime') + ':' + str(csmith_root / 'build' / 'runtime')
    creduce_result: subprocess.CompletedProcess = subprocess.run(['creduce', '__interesting.py', '__prog.c'],
                                                                 env=creduce_environment)
    if creduce_result.returncode != 0:
        print('creduce failed')
        return False
    return True


class ExecutionStatus(Enum):
    NO_EFFECT = auto()
    DIFFERENT_BINARIES_SAME_RESULT = auto()
    COMPILE_FAIL_KILL = auto()
    COMPILE_TIMEOUT_KILL = auto()
    RUN_FAIL_KILL = auto()
    RUN_TIMEOUT_KILL = auto()
    MISCOMPILATION_KILL = auto()


def select_mutants(mutation_tree: MutationTree,
                   unkilled_mutants_not_yet_tried: Set[int],
                   num_to_select: int) -> List[int]:
    available_mutants: Set[int] = set(unkilled_mutants_not_yet_tried)
    result: List[int] = []
    while len(available_mutants) > 0 and len(result) < num_to_select:
        selected_mutant = list(available_mutants)[random.randrange(0, len(available_mutants))]
        result.append(selected_mutant)
        available_mutants = available_mutants.difference(
            set(mutation_tree.get_incompatible_mutation_ids(selected_mutant)))
    return result


def try_to_kill_mutants(csmith_root: Path,
                        compiler_executable: Path,
                        program_stats: GeneratedProgramStats,
                        selected_mutants: List[int]) -> ExecutionStatus:
    try:
        cmd: List[str] = [compiler_executable, "-O3", "-I", csmith_root / "runtime", "-I",
                          csmith_root / "build" / "runtime", "__prog.c", "-o", "prog_mutated"]
        mutated_environment: Dict[str, str] = os.environ.copy()
        mutated_environment['DREDD_ENABLED_MUTATION'] = ",".join([str(m) for m in selected_mutants])
        result = subprocess.run(cmd, timeout=max(5.0, 5.0 * program_stats.compile_time), capture_output=True,
                                env=mutated_environment)
    except subprocess.TimeoutExpired:
        print("WEAK KILL: Compilation with mutated compiler timed out.")
        return ExecutionStatus.COMPILE_TIMEOUT_KILL

    if result.returncode != 0:
        print("WEAK KILL: Compilation with mutated compiler failed.")
        return ExecutionStatus.COMPILE_FAIL_KILL

    print("Compilation with mutated compiler succeeded")
    if hash_file('prog_mutated') == program_stats.executable_hash:
        print("Binaries are the same - not interesting")
        return ExecutionStatus.NO_EFFECT

    print("Different binaries!")
    try:
        cmd: List[str] = ["./prog_mutated"]
        result = subprocess.run(cmd, timeout=max(5.0, 10.0 * program_stats.execution_time), capture_output=True)
        if result.returncode != 0:
            print("STRONG KILL: Execution of program compiled with mutated compiler failed.")
            return ExecutionStatus.RUN_FAIL_KILL
        if result.stdout.decode('utf-8') != program_stats.expected_output:
            print("VERY STRONG KILL: Execution results from program compiled with mutated compiler are different!")
            return ExecutionStatus.MISCOMPILATION_KILL

    except subprocess.TimeoutExpired:
        print("STRONG KILL: Execution of program compiled with mutated compiler timed out.")
        return ExecutionStatus.RUN_TIMEOUT_KILL

    print("Same execution results - not interesting")
    return ExecutionStatus.DIFFERENT_BINARIES_SAME_RESULT


def is_killed_by_reduced_test_case(mutant: int,
                                   csmith_root: Path,
                                   compiler_executable: Path) -> bool:
    generate_interestingness_test(csmith_root=csmith_root,
                                  compiler_executable=compiler_executable,
                                  mutant=mutant)
    if subprocess.run(['./__interesting.py']).returncode == 0:
        print(f'Reduced file kills mutant {mutant}')
        return True
    print(f'Reduced file does not kill mutant {mutant}')
    return False


def consolodate_kill(mutant: int,
                     execution_status: ExecutionStatus,
                     csmith_root: Path,
                     compiler_executable: Path,
                     program_stats: GeneratedProgramStats,
                     mutation_tree: MutationTree) -> Set[int]:
    print("Consolodating kills")
    miscompilation_kills_to_reduce: List[int] = []
    result: Set[int] = set()
    if execution_status == ExecutionStatus.MISCOMPILATION_KILL:
        miscompilation_kills_to_reduce.append(mutant)
    else:
        result.add(mutant)

    print(f"Found {len(result)} follow-on kills that are not miscompilations")

    for relative in mutation_tree.get_incompatible_mutation_ids(mutant):
        if relative == mutant:
            continue

        result_for_relative: ExecutionStatus = try_to_kill_mutants(csmith_root=csmith_root,
                                                                   compiler_executable=compiler_executable,
                                                                   program_stats=program_stats,
                                                                   selected_mutants=[relative])
        sys.stdout.flush()
        if result_for_relative == ExecutionStatus.NO_EFFECT or result_for_relative \
                == ExecutionStatus.DIFFERENT_BINARIES_SAME_RESULT:
            continue
        if result_for_relative == ExecutionStatus.MISCOMPILATION_KILL:
            miscompilation_kills_to_reduce.append(relative)
        else:
            result.add(relative)

    print(f"Found {len(miscompilation_kills_to_reduce)} miscompilation inducing mutant(s) to reduce")
    while len(miscompilation_kills_to_reduce) > 0:
        mutant: int = miscompilation_kills_to_reduce.pop()
        if not reduce_very_strong_kill(csmith_root=csmith_root, compiler_executable=compiler_executable, mutant=mutant):
            continue
        killed_by_reduced_test_case: List[int] = [mutant]
        index: int = 0
        while index < len(miscompilation_kills_to_reduce):
            follow_on_mutant: int = miscompilation_kills_to_reduce[index]
            if is_killed_by_reduced_test_case(csmith_root=csmith_root,
                                              compiler_executable=compiler_executable,
                                              mutant=follow_on_mutant):
                killed_by_reduced_test_case.append(follow_on_mutant)
                miscompilation_kills_to_reduce.pop(index)
            else:
                index += 1
        print(f"Found {len(killed_by_reduced_test_case)} miscompilation kills from a reduced test case")
        shutil.move(src='__prog.c', dst=f'__kills_{"_".join([str(m) for m in killed_by_reduced_test_case])}.c')
        for m in killed_by_reduced_test_case:
            result.add(m)
    return result


def search_for_kills_in_mutant_selection(csmith_root: Path,
                                         compiler_executable: Path,
                                         program_stats: GeneratedProgramStats,
                                         mutation_tree: MutationTree,
                                         selected_mutants: List[int]) -> Set[int]:
    assert len(selected_mutants) > 0
    print(f"Searching for kills among {len(selected_mutants)} mutant(s)")
    execution_status: ExecutionStatus = try_to_kill_mutants(csmith_root=csmith_root,
                                                            compiler_executable=compiler_executable,
                                                            program_stats=program_stats,
                                                            selected_mutants=selected_mutants)
    sys.stdout.flush()
    if execution_status == ExecutionStatus.NO_EFFECT or \
            execution_status == ExecutionStatus.DIFFERENT_BINARIES_SAME_RESULT:
        return set()

    if len(selected_mutants) == 1:
        return consolodate_kill(mutant=selected_mutants[0],
                                execution_status=execution_status,
                                csmith_root=csmith_root,
                                compiler_executable=compiler_executable,
                                program_stats=program_stats,
                                mutation_tree=mutation_tree)

    midpoint: int = int(len(selected_mutants) / 2)
    selected_mutants_lhs: List[int] = selected_mutants[0:midpoint]
    selected_mutants_rhs: List[int] = selected_mutants[midpoint:]

    result: Set[int] = search_for_kills_in_mutant_selection(csmith_root=csmith_root,
                                                            compiler_executable=compiler_executable,
                                                            program_stats=program_stats,
                                                            mutation_tree=mutation_tree,
                                                            selected_mutants=selected_mutants_lhs)
    for m in result:
        if m in selected_mutants_rhs:
            selected_mutants_rhs.remove(m)

    for m in search_for_kills_in_mutant_selection(csmith_root=csmith_root,
                                                  compiler_executable=compiler_executable,
                                                  program_stats=program_stats,
                                                  mutation_tree=mutation_tree,
                                                  selected_mutants=selected_mutants_rhs):
        result.add(m)
    print(f"Finished searching for kills among {len(selected_mutants)} mutant(s)")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mutation_info_file",
                        help="File containing information about mutations.",
                        type=Path)
    parser.add_argument("compiler_executable",
                        help="Path to the executable for the Dredd-mutated compiler.",
                        type=Path)
    parser.add_argument("csmith_root", help="Path to a checkout of Csmith, assuming that it has been built under "
                                            "'build' beneath this directory.",
                        type=Path)
    parser.add_argument("--max_consecutive_failed_attempts_per_program",
                        default=10,
                        help="The number of times a given generated program will be compiled and executed in a row "
                             "with no mutation being killed.",
                        type=int)
    parser.add_argument("--max_attempts_per_program",
                        default=100,
                        help="The number of times a given program will be compiled and executed to look for kills ("
                             "excluding work done investigating kills) before moving on to the next program.",
                        type=int)
    parser.add_argument("--num_simultaneous_mutations",
                        default=64,
                        help="The number of mutations that will be enabled simultaneously when looking for kills.",
                        type=int)
    args = parser.parse_args()

    print("Building the mutation tree...")
    with open(args.mutation_info_file, 'r') as json_input:
        mutation_tree = MutationTree(json.load(json_input))
    print("Built!")
    unkilled_mutants_not_yet_tried: Set[int] = set(range(0, mutation_tree.num_mutations))
    unkilled_mutants_already_tried: Set[int] = set()
    killed_mutants: Set[int] = set()

    while True:
        print("Generating a program...")
        program_stats: GeneratedProgramStats = generate_a_program(csmith_root=args.csmith_root,
                                                                  compiler_executable=args.compiler_executable)
        print("Generated!")
        num_attempts_for_current_program: int = 0
        num_consecutive_failed_attempts_for_current_program: int = 0
        while num_attempts_for_current_program < args.max_attempts_per_program and \
                num_consecutive_failed_attempts_for_current_program < args.max_consecutive_failed_attempts_per_program:
            selected_mutants = select_mutants(mutation_tree=mutation_tree,
                                              unkilled_mutants_not_yet_tried=unkilled_mutants_not_yet_tried,
                                              num_to_select=args.num_simultaneous_mutations)
            newly_killed: Set[int] = search_for_kills_in_mutant_selection(csmith_root=args.csmith_root,
                                                                          compiler_executable=args.compiler_executable,
                                                                          program_stats=program_stats,
                                                                          mutation_tree=mutation_tree,
                                                                          selected_mutants=selected_mutants)
            print(f"Found {len(newly_killed)} newly-killed mutant(s)")
            for m in newly_killed:
                killed_mutants.add(m)
                if m in unkilled_mutants_already_tried:
                    unkilled_mutants_already_tried.remove(m)
                if m in unkilled_mutants_not_yet_tried:
                    unkilled_mutants_not_yet_tried.remove(m)
            for m in selected_mutants:
                if m in unkilled_mutants_not_yet_tried:
                    unkilled_mutants_not_yet_tried.remove(m)
                    unkilled_mutants_already_tried.add(m)
            assert len(killed_mutants) + len(unkilled_mutants_already_tried) + len(unkilled_mutants_not_yet_tried) \
                   == mutation_tree.num_mutations
            sys.stdout.flush()
            if len(newly_killed) == 0:
                num_consecutive_failed_attempts_for_current_program += 1
            else:
                num_consecutive_failed_attempts_for_current_program = 0
            num_attempts_for_current_program += 1


if __name__ == '__main__':
    main()
