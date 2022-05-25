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

# iwyu does not give a non-zero exit code.
# We use awk.

# Allow spaces to split cpp files:
# shellcheck disable=SC2046
iwyu_tool.py \
  -p="${COMPILE_COMMANDS}" \
  $(dredd_cc_files.sh) \
  -- \
  -Xiwyu --no_fwd_decls \
  -Xiwyu --mapping_file="${DREDD_REPO_ROOT}/src/iwyu.imp" \
  | awk '/should add these lines:/{ exit_code = 1; } { print; } END { if (exit_code) { print "include-what-you-use error; see above."; exit 1; } }'
