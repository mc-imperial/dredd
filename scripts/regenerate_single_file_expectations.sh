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

  cd ${DREDD_REPO_ROOT}
  
  # Ensure that Dredd is in its installed location. This depends on a
  # debug build being available
  cp temp/build-Debug/src/dredd/dredd ${DREDD_INSTALLED_EXECUTABLE}

  # Avoid copying Dredd to its installed location when invoking the script that
  # checks a single test.
  export DREDD_SKIP_COPY_EXECUTABLE=1
  
  # Consider each single-file test case
  for f in `ls test/single_file/*.cc test/single_file/*.c`
  do
    regenerate_one_single_file_expectation.sh ${f}
  done
fi
