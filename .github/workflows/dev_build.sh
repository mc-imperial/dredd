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


help | head

uname

case "$(uname)" in
"Linux")
  NINJA_OS="linux"
  df -h
  sudo swapoff -a
  sudo rm -f /swapfile
  sudo apt clean
  # shellcheck disable=SC2046
  docker rmi $(docker image ls -aq)
  df -h
  ;;

*)
  echo "Unknown OS: only Linux is supported for the dev_build workflow"
  exit 1
  ;;
esac

# Get cmake-format and cmake-lint
pip install cmakelang

export PATH="${HOME}/bin:$PATH"
mkdir -p "${HOME}/bin"
pushd "${HOME}/bin"
  # Install ninja.
  curl -fsSL -o ninja-build.zip "https://github.com/ninja-build/ninja/releases/download/v1.11.0/ninja-${NINJA_OS}.zip"
  unzip ninja-build.zip
  ls
popd

# Install clang.
DREDD_LLVM_TAG=$(./scripts/llvm_tag.sh)
pushd ./third_party/clang+llvm
  curl -fsSL -o clang+llvm.tar.xz "https://github.com/llvm/llvm-project/releases/download/llvmorg-${DREDD_LLVM_TAG}/clang+llvm-${DREDD_LLVM_TAG}-x86_64-linux-gnu-ubuntu-22.04.tar.xz"
  tar xf clang+llvm.tar.xz
  mv clang+llvm-${DREDD_LLVM_TAG}-x86_64-linux-gnu-ubuntu-22.04/* .
  rm clang+llvm.tar.xz
popd

# Source the dev shell to download clang-tidy and other tools.
# Developers should *run* the dev shell, but we want to continue executing this script.
export DREDD_SKIP_BASH=1

source ./dev_shell.sh.template

check_all.sh
