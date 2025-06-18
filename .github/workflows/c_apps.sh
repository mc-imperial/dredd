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

  sudo apt update
  sudo apt install -y llvm-17 clang-17 clang-tidy-17 clang-format-17 libclang-17-dev

  # Free up some space
  df -h
  sudo swapoff -a
  sudo rm -f /swapfile
  sudo apt clean
  df -h
  ;;

*)
  echo "Unknown OS: only Linux is supported for the c_apps workflow"
  exit 1
  ;;
esac

export PATH="${HOME}/bin:$PATH"
mkdir -p "${HOME}/bin"
pushd "${HOME}/bin"
  # Install ninja.
  curl -fsSL -o ninja-build.zip "https://github.com/ninja-build/ninja/releases/download/v1.11.0/ninja-${NINJA_OS}.zip"
  unzip ninja-build.zip
  ls
popd

DREDD_ROOT=$(pwd)

DREDD_CLANG_LLVM_DIR="/usr/lib/llvm-17"
export PATH="${DREDD_CLANG_LLVM_DIR}/bin:$PATH"

export CC=clang
export CXX=clang++

which ${CC}
which ${CXX}

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DDREDD_CLANG_LLVM_DIR="${DREDD_CLANG_LLVM_DIR}"
cmake --build build --config Debug

# Check that dredd works on some projects
DREDD_EXECUTABLE="${DREDD_ROOT}/build/src/dredd/dredd"

echo "Curl"
date

git clone https://github.com/curl/curl.git
pushd curl
  git reset --hard curl-7_84_0
  mkdir build
  pushd build
    cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
  popd
  FILES=()
  for f in $(find src -name "*.c")
  do
      FILES+=("${f}")
  done

  "${DREDD_EXECUTABLE}" --mutation-info-file temp.json -p "build" "${FILES[@]}"
  pushd build
    ninja
    # TODO: run some tests
  popd
popd

echo "zstd"
date

git clone https://github.com/facebook/zstd.git
pushd zstd
  git reset --hard v1.4.10
  mkdir temp
  pushd temp
    # Generate a compilation database
    cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../build/cmake
  popd
  # Build non-mutated zstd
  CFLAGS=-O0 make zstd-release
  # Use the compiled zstd binary as a target for compression, and compress it.
  cp ./programs/zstd tocompress
  ./zstd tocompress -o normal
  # Mutate all the source files in the lib directory of zstd
  FILES=()
  for f in $(find lib -name "*.c")
  do
    FILES+=("${f}")
  done
  "${DREDD_EXECUTABLE}" --mutation-info-file temp.json -p "temp" "${FILES[@]}"
  # Build mutated zstd
  make clean
  CFLAGS=-O0 make zstd-release
  # Use it to compress the original (non-mutated) zstd binary
  ./zstd tocompress -o mutated
  # The results obtained using the original and mutated versions of zstd should
  # be identical, since no mutations were enabled.
  diff normal mutated
popd

echo "Finished"
date
