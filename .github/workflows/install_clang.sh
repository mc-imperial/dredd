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

set -x
set -e
set -u

case "$(uname)" in
"Linux")
  LLVM_RELEASE_OS="${OS}"
  ;;

"Darwin")
  LLVM_RELEASE_OS="Mac"
  ;;

"MINGW"*|"MSYS_NT"*)
  LLVM_RELEASE_OS="Windows"
  ;;

*)
  echo "Unknown OS"
  exit 1
  ;;
esac

DREDD_LLVM_TAG=$(./scripts/llvm_tag.sh)
pushd ./third_party/clang+llvm
  curl -fsSL -o clang+llvm.zip "https://github.com/mc-imperial/build-clang/releases/download/llvmorg-${DREDD_LLVM_TAG}/build-clang-llvmorg-${DREDD_LLVM_TAG}-${LLVM_RELEASE_OS}_x64_Release.zip"
  unzip clang+llvm.zip
  rm clang+llvm.zip
popd
