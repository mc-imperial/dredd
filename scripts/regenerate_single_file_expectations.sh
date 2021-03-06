#!/usr/bin/env bash

# Copyright 2022 The Dredd Project Authors
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

set -e
set -u
set -x

if [ -z "${DREDD_SKIP_CHECK_COMPILE_COMMANDS+x}" ]
then
  DREDD_INSTALLED_EXECUTABLE=${DREDD_REPO_ROOT}/third_party/clang+llvm-13.0.1/bin/dredd
  # Move to the temporary directory
  cd "${DREDD_REPO_ROOT}"
  cd temp/
  # Ensure that Dredd is in its installed location. This depends on a
  # debug build being available
  cp build-Debug/src/dredd/dredd ${DREDD_INSTALLED_EXECUTABLE}
  # Consider each single-file test case
  for f in `ls ${DREDD_REPO_ROOT}/test/single_file/*.cc`
  do
    # Copy the single-file test case to the temporary directory so
    # that it can be mutated without affecting the original
    cp $f .
    copy_of_f=$(basename $f)      
    # Mutate the test case using Dredd
    ${DREDD_INSTALLED_EXECUTABLE} ${copy_of_f} --
    # Check that the mutated file compiles
    ${CXX} -c ${copy_of_f}
    # Copy the mutated file so that it becomes the new test expectation
    cp ${copy_of_f} ${f%.*}.expected
    # Clean up
    rm ${copy_of_f}
    rm ${copy_of_f%.*}.o
  done
fi
