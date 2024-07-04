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

COMPILE_COMMANDS="$(realpath "${1}")"
shift

cd "${DREDD_REPO_ROOT}"

cppcheck \
  --project="${COMPILE_COMMANDS}" \
  --file-filter="$(pwd)/src/"'*' \
  --check-level=exhaustive \
  --error-exitcode=2 \
  --enable=all \
  --inline-suppr \
  --suppress=unusedFunction \
  --suppress=missingIncludeSystem \
  --suppress=missingInclude \
  --suppress=unmatchedSuppression \
  --suppress=syntaxError \
  --suppress=useStlAlgorithm \
  --suppress=preprocessorErrorDirective
