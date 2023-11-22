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

cd "${DREDD_REPO_ROOT}"

if ! cmake-format --version
then
  echo "cmake-format is not installed."
  exit 1
fi

dredd_source_files.sh | xargs -t clang-format --dry-run --Werror
for f in $(dredd_cmake_files.sh)
do
    cmake-format --first-comment-is-literal TRUE --check "$f"
done

examples_source_files.sh | xargs -t clang-format --dry-run --Werror
for f in $(examples_cmake_files.sh)
do
    cmake-format --first-comment-is-literal TRUE --check "$f"
done

clang-format ./src/libdredd/include/libdredd/protobufs/dredd.proto --dry-run --Werror
