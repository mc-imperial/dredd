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

import glob
import os
import shutil
import subprocess
import sys

from pathlib import Path

DREDD_REPO_ROOT = os.environ['DREDD_REPO_ROOT']
DREDD_EXECUTABLE = Path(DREDD_REPO_ROOT, 'temp', 'build-Debug', 'src', 'dredd', 'dredd') if 'DREDD_EXECUTABLE' not in os.environ else os.environ['DREDD_EXECUTABLE']
test_directory = Path(DREDD_REPO_ROOT, sys.argv[1])

# Back up current directory and move to a temporary directory
original_dir = os.getcwd()

bespoke_test_work_dir = Path(DREDD_REPO_ROOT, 'temp', 'bespoke_test_work_dir')
if Path(bespoke_test_work_dir).exists():
    print(f"Error: bespoke work directory {bespoke_test_work_dir} must not already exist")
    sys.exit(1)

Path(bespoke_test_work_dir).mkdir(exist_ok=False)
os.chdir(bespoke_test_work_dir)

if not os.path.exists(test_directory / 'test.py'):
    print(f"No 'test.py' file found for bespoke test {test_directory}")
    sys.exit(2)

# Copy the test files into the temporary directory
for filename in glob.glob(str(test_directory) + os.sep + '*'):
    shutil.copy(src=filename,
                dst=os.path.basename(filename))

# Run the bespoke test
result = subprocess.run(["python", "test.py"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

if result.returncode != 0:
    print(f"Bespoke test {test_directory} failed with exit code {result.returncode}")
    print(result.stdout.decode('utf-8'))
    print(result.stderr.decode('utf-8'))
    sys.exit(3)

print("Success!")

# Change back to current directory and clean up
os.chdir(original_dir)
shutil.rmtree(bespoke_test_work_dir)
