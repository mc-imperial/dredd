#!/usr/bin/env python3

# Copyright 2024 The Dredd Project Authors
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

import os
import shutil
import subprocess
import sys

from pathlib import Path

DREDD_REPO_ROOT = os.environ['DREDD_REPO_ROOT']
DREDD_INSTALLED_EXECUTABLE = Path(DREDD_REPO_ROOT, 'third_party', 'clang+llvm', 'bin', 'dredd')
test_directory = Path(DREDD_REPO_ROOT, sys.argv[1])
test_is_cxx = os.path.exists(test_directory / 'harness.cc')
extension = 'cc' if test_is_cxx else 'c'

# Back up current directory and move to a temporary directory
original_dir = os.getcwd()
os.chdir(Path(DREDD_REPO_ROOT, 'temp'))

if 'DREDD_SKIP_COPY_EXECUTABLE' not in os.environ or os.environ['DREDD_SKIP_COPY_EXECUTABLE'] != '1':
    # Ensure that Dredd is in its installed location. This depends on a
    # debug build being available
    shutil.copy(src=Path('build-Debug', 'src', 'dredd', 'dredd'),
                dst=DREDD_INSTALLED_EXECUTABLE)

# Copy the test files into the temporary directory for mutation
shutil.copy(src=test_directory / f'harness.{extension}',
            dst=f'harness.{extension}')
shutil.copy(src=test_directory / f'tomutate.{extension}',
            dst=f'tomutate.{extension}')

# Get the expected output from the original test, and the set of expected mutant outputs.
expected_original_output = open(test_directory / 'original.txt', 'r').read().strip()
expected_mutant_outputs = set()
for line in open(test_directory / 'mutants.txt', 'r').readlines():
    component = line.split(';')[0].strip()
    if component == '':
        continue
    expected_mutant_outputs.add(component)

# Mutate the program using Dredd.
cmd = [DREDD_INSTALLED_EXECUTABLE, '--mutation-info-file', 'temp.json', f'tomutate.{extension}', '--']
dredd_result = subprocess.run(cmd)
if dredd_result.returncode != 0:
    print("Dredd failed.")
    print(dredd_result.stderr.decode('utf-8'))
    sys.exit(1)

# Compile the mutated program.
COMPILER = os.environ['CXX'] if test_is_cxx else os.environ['CC']
EXTRA_COMPILER_ARGS = (os.environ.get('DREDD_EXTRA_CXX_ARGS', '') if test_is_cxx else os.environ.get('DREDD_EXTRA_C_ARGS', '')).split()
cmd = [COMPILER] + EXTRA_COMPILER_ARGS + [f'harness.{extension}', f'tomutate.{extension}', '-o', 'test_executable']
compile_result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
if compile_result.returncode != 0:
    print("Error compiling mutated file.")
    print(compile_result.stderr.decode('utf-8'))
    sys.exit(2)

# Run the compiled mutated program with no mutants enabled, and check the result is as expected.
cmd = [os.getcwd() + os.sep + 'test_executable']
original_result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
if original_result.returncode != 0:
    print("Error running non-mutated executable")
    print(original_result.stderr.decode('utf-8'))
    sys.exit(3)
actual_original_output = original_result.stdout.decode('utf-8').strip()
if expected_original_output != actual_original_output:
    print("Unexpected output from non-mutated execution.")
    print("Expected: " + expected_original_output)
    print("  Actual: " + actual_original_output)
    sys.exit(4)

# Get the number of mutants that are available.
cmd = ['python3', Path(DREDD_REPO_ROOT, 'scripts', 'query_mutant_info.py'), '--largest-mutant-id', 'temp.json']
largest_mutant_id_result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
if largest_mutant_id_result.returncode != 0:
    print("Error finding largest mutant id.")
    print(largest_mutant_id_result.stderr.decode('utf-8'))
    sys.exit(5)
largest_mutant_id = int(largest_mutant_id_result.stdout.decode('utf-8'))

# Run mutant-by-mutant until all of the expected outputs are observed.
current_mutant_id = 0
while current_mutant_id <= largest_mutant_id and len(expected_mutant_outputs) > 0:
    cmd = [os.getcwd() + os.sep + 'test_executable']
    mutant_environment = os.environ.copy()
    mutant_environment['DREDD_ENABLED_MUTATION'] = str(current_mutant_id)
    mutant_result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=mutant_environment)
    if mutant_result.returncode == 0:
        mutant_output = mutant_result.stdout.decode('utf-8').strip()
        if mutant_output in expected_mutant_outputs:
            expected_mutant_outputs.remove(mutant_output)
    current_mutant_id += 1

# Complain if not all of the expected outputs are observed.
if len(expected_mutant_outputs) > 0:
    print("Error: there were unmatched expected mutant outputs:")
    print(expected_mutant_outputs)
    sys.exit(6)

print("Success!")

# Clean up and change back to current directory
os.remove(f'harness.{extension}')
os.remove(f'tomutate.{extension}')
os.remove('temp.json')
if os.name == 'nt':
    os.remove('test_executable.exe')
else:
    os.remove('test_executable')
os.chdir(original_dir)
