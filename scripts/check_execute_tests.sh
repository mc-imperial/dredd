#!/usr/bin/env bash

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

set -e
set -u
set -x

if [ -z "${DREDD_SKIP_CHECK_COMPILE_COMMANDS+x}" ]
then
  if [ -z "${DREDD_EXECUTABLE+x}" ]
  then
    DREDD_EXECUTABLE=${DREDD_REPO_ROOT}/temp/build-Debug/src/dredd/dredd
  fi
  
  cd "${DREDD_REPO_ROOT}"

  # Consider each single-file test case
  for f in test/execute/*
  do
    [[ -e "$f" ]] || break
    if [ -d "${f}" ]
    then
      check_one_execute_test.py "${f}"
    fi
  done
fi
