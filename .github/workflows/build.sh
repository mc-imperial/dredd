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

# Old bash versions can't expand empty arrays, so we always include at least this option.
CMAKE_OPTIONS=("-DCMAKE_OSX_ARCHITECTURES=x86_64")

help | head

uname

DREDD_LLVM_TAG=$(./scripts/llvm_tag.sh)

case "$(uname)" in
"Linux")
  NINJA_OS="linux"
  LLVM_RELEASE_OS="${OS}"
  # Provided by build.yml.
  export CC="${LINUX_CC}"
  export CXX="${LINUX_CXX}"
  # Free up some space
  df -h
  sudo apt clean
  # shellcheck disable=SC2046
  docker rmi -f $(docker image ls -aq)
  sudo rm -rf /usr/share/dotnet /usr/local/lib/android /opt/ghc
  df -h
  ;;

"Darwin")
  NINJA_OS="mac"
  LLVM_RELEASE_OS="Mac"
  ;;

"MINGW"*|"MSYS_NT"*)
  NINJA_OS="win"
  LLVM_RELEASE_OS="Windows"
  CMAKE_OPTIONS+=("-DCMAKE_C_COMPILER=cl.exe" "-DCMAKE_CXX_COMPILER=cl.exe")
  choco install zip
  ;;

*)
  echo "Unknown OS"
  exit 1
  ;;
esac

# Install clang.
pushd ./third_party/clang+llvm
curl -fsSL -o clang+llvm.zip "https://github.com/mc-imperial/build-clang/releases/download/llvmorg-${DREDD_LLVM_TAG}/build-clang-llvmorg-${DREDD_LLVM_TAG}-${LLVM_RELEASE_OS}_x64_Release.zip"
unzip clang+llvm.zip
rm clang+llvm.zip
popd

export PATH="${HOME}/bin:$PATH"

mkdir -p "${HOME}/bin"

pushd "${HOME}/bin"

  # Install ninja.
  curl -fsSL -o ninja-build.zip "https://github.com/ninja-build/ninja/releases/download/v1.11.0/ninja-${NINJA_OS}.zip"
  unzip ninja-build.zip

  ls

popd

case "$(uname)" in
"Linux")

  # On Linux, source the dev shell to download clang-tidy and other tools.
  # Developers should *run* the dev shell, but we want to continue executing this script.
  export DREDD_SKIP_COMPILER_SET=1
  export DREDD_SKIP_BASH=1

  source ./dev_shell.sh.template

  ;;

"Darwin")
  ;;

"MINGW"*|"MSYS_NT"*)
  ;;

*)
  echo "Unknown OS"
  exit 1
  ;;
esac

mkdir -p build
pushd build
  cmake -G Ninja .. -DCMAKE_BUILD_TYPE="${CONFIG}" "${CMAKE_OPTIONS[@]}"
  cmake --build . --config "${CONFIG}"
  cmake -DCMAKE_INSTALL_PREFIX=./install -DBUILD_TYPE="${CONFIG}" -P cmake_install.cmake
  # Run the unit tests
  ./src/libdreddtest/libdreddtest
popd


case "$(uname)" in
"MINGW"*|"MSYS_NT"*)
  # The following single-file tests give different expected results on Windows
  # due to differences in how certain builtin types, such as size_t and
  # uint64_t, expand. For simplicity, remove them before running single-file
  # tests on Windows.
  rm test/single_file/initializer_list.cc
  rm test/single_file/initializer_list.cc.expected
  rm test/single_file/initializer_list.cc.noopt.expected
  rm test/single_file/add_type_aliases.c
  rm test/single_file/add_type_aliases.c.expected
  rm test/single_file/add_type_aliases.c.noopt.expected
  rm test/single_file/add_type_aliases.cc
  rm test/single_file/add_type_aliases.cc.expected
  rm test/single_file/add_type_aliases.cc.noopt.expected
  rm test/single_file/positive_int_as_minus_one.c
  rm test/single_file/positive_int_as_minus_one.c.expected
  rm test/single_file/positive_int_as_minus_one.c.noopt.expected
  rm test/single_file/positive_int_as_minus_one.cc
  rm test/single_file/positive_int_as_minus_one.cc.expected
  rm test/single_file/positive_int_as_minus_one.cc.noopt.expected
  ;;

*)
  ;;
esac


case "$(uname)" in
"Linux")
  # On Linux, run a few extra analyzes using the compile_commands.json file.
  check_compile_commands.sh build/compile_commands.json
  ;;

"Darwin"|"MINGW"*|"MSYS_NT"*)
  # On Mac and Windows, run the single-file tests
  cp build/src/dredd/dredd third_party/clang+llvm/bin/
  export DREDD_REPO_ROOT=$(pwd)
  export PATH=${PATH}:${DREDD_REPO_ROOT}/scripts
  export PATH=${DREDD_REPO_ROOT}/third_party/clang+llvm/bin:${PATH}
  export CC=clang
  export CXX=clang++
  DREDD_SKIP_COPY_EXECUTABLE=1 ./scripts/check_single_file_tests.sh
  ;;

*)
  echo "Unknown OS"
  exit 1
  ;;
esac
