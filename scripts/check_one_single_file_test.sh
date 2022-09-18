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

DREDD_INSTALLED_EXECUTABLE=${DREDD_REPO_ROOT}/third_party/clang+llvm-13.0.1/bin/dredd

# Move to the temporary directory
cd "${DREDD_REPO_ROOT}"
cd temp/

if [ -z "${DREDD_SKIP_COPY_EXECUTABLE+x}" ]
then
  # Ensure that Dredd is in its installed location. This depends on a
  # debug build being available
  cp build-Debug/src/dredd/dredd ${DREDD_INSTALLED_EXECUTABLE}
fi

f="${DREDD_REPO_ROOT}/${1}"

for opt in "opt" "noopt"
do
  DREDD_EXTRA_ARGS=""
  DREDD_EXPECTED_FILE=""
  if [ ${opt} == "noopt" ]
  then
    DREDD_EXTRA_ARGS="--no-mutation-opts"
    DREDD_EXPECTED_FILE="$f.noopt.expected"
  else
    DREDD_EXPECTED_FILE="$f.expected"
  fi

  # Copy the single-file test case to the temporary directory so
  # that it can be mutated without affecting the original
  cp $f .
  copy_of_f=$(basename $f)

  # Mutate the test case using Dredd with optimisations
  ${DREDD_INSTALLED_EXECUTABLE} ${DREDD_EXTRA_ARGS} --mutation-info-file temp.json ${copy_of_f} --

  if [ -z "${DREDD_REGENERATE_TEST_CASE+x}" ]
  then
    # Check that the mutated test case is as expected
    diff ${copy_of_f} ${DREDD_EXPECTED_FILE}
  fi

  # Check that the mutated file compiles
  if [[ $f == *.cc ]]
  then
    ${CXX} -c ${copy_of_f}
  else
    ${CC} -c ${copy_of_f}
  fi

  if [ "${DREDD_REGENERATE_TEST_CASE+x}" ]
  then
    # Copy the mutated file so that it becomes the new test expectation
    cp ${copy_of_f} ${DREDD_EXPECTED_FILE}
  fi

  # Clean up
  rm ${copy_of_f}
  rm ${copy_of_f%.*}.o

done
