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

case "$(uname)" in
"Linux")
  NINJA_OS="linux"
  df -h
  sudo apt clean
  # shellcheck disable=SC2046
  docker rmi -f $(docker image ls -aq)
  sudo rm -rf /usr/share/dotnet /usr/local/lib/android /opt/hostedtoolcache/boost /opt/ghc
  df -h

  # Provided by build.yml.
  export CC="${LINUX_CC}"
  export CXX="${LINUX_CXX}"

  # Install clang.
  CLANG_VERSION=clang+llvm-13.0.1-x86_64-linux-gnu-ubuntu-18.04
  curl -fsSL -o ${CLANG_VERSION}.tar.xz "https://github.com/llvm/llvm-project/releases/download/llvmorg-13.0.1/${CLANG_VERSION}.tar.xz"
  tar xf "${CLANG_VERSION}".tar.xz
  mv "${CLANG_VERSION}" ./third_party/clang+llvm-13.0.1
  ;;

"Darwin")
  NINJA_OS="mac"
  # Install clang.
  CLANG_VERSION=clang+llvm-13.0.1-x86_64-apple-darwin
  curl -fsSL -o ${CLANG_VERSION}.tar.xz "https://github.com/llvm/llvm-project/releases/download/llvmorg-13.0.1/${CLANG_VERSION}.tar.xz"
  tar xf "${CLANG_VERSION}".tar.xz
  mv "${CLANG_VERSION}" ./third_party/clang+llvm-13.0.1
  ;;

*)
  echo "Unknown OS"
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
"Linux")
  # On Linux, run a few extra analyzes using the compile_commands.json file.
  check_compile_commands.sh build/compile_commands.json
  ;;

"Darwin")
  ;;

*)
  echo "Unknown OS"
  exit 1
  ;;
esac
