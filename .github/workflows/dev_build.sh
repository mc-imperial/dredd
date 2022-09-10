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

echo "Starting devbuild"
date

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
CLANG_VERSION=clang+llvm-13.0.1-x86_64-linux-gnu-ubuntu-18.04
curl -fsSL -o ${CLANG_VERSION}.tar.xz "https://github.com/llvm/llvm-project/releases/download/llvmorg-13.0.1/${CLANG_VERSION}.tar.xz"
tar xf "${CLANG_VERSION}".tar.xz
mv "${CLANG_VERSION}" ./third_party/clang+llvm-13.0.1
rm ${CLANG_VERSION}.tar.xz

# Source the dev shell to download clang-tidy and other tools.
# Developers should *run* the dev shell, but we want to continue executing this script.
export DREDD_SKIP_BASH=1

source ./dev_shell.sh.template

echo "Running check_all"
date

check_all.sh

# Check that dredd works on some projects
DREDD_ROOT=`pwd`
cp ${DREDD_ROOT}/temp/build-Debug/src/dredd/dredd ${DREDD_ROOT}/third_party/clang+llvm-13.0.1/bin/dredd

echo "examples/simple/pi.cc: check that we can build the simple example"
date

${DREDD_ROOT}/third_party/clang+llvm-13.0.1/bin/dredd examples/simple/pi.cc
clang++ examples/simple/pi.cc -o examples/simple/pi
diff <(./examples/simple/pi) <(echo "3.14159")

echo "examples/math: check that the tests pass after mutating the library"
date

pushd examples/math
  mkdir build
  pushd build
    cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
    ../mutate.sh
    cmake --build .
    ./mathtest/mathtest
  popd
popd

echo "Curl"
date

git clone https://github.com/curl/curl.git
pushd curl
  git reset --hard curl-7_84_0
  mkdir build
  pushd build
    cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
  popd
  FILES=""
  for f in `find src -name "*.c"`
  do
      FILES="${FILES} ${f}"
  done
  ${DREDD_ROOT}/third_party/clang+llvm-13.0.1/bin/dredd -p ${DREDD_ROOT}/curl/build/compile_commands.json ${FILES}
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
  FILES=""
  for f in `find lib -name "*.c"`
  do
    FILES="${FILES} ${f}"
  done
  ${DREDD_ROOT}/third_party/clang+llvm-13.0.1/bin/dredd -p ${DREDD_ROOT}/zstd/temp/compile_commands.json ${FILES}
  # Build mutated zstd
  make clean
  CFLAGS=-O0 make zstd-release
  # Use it to compress the original (non-mutated) zstd binary
  ./zstd tocompress -o mutated
  # The results obtained using the original and mutated versions of zstd should
  # be identical, since no mutations were enabled.
  diff normal mutated
popd

echo "SPIRV-Tools validator: check that the tests pass after mutating the validator"
date

git clone https://github.com/KhronosGroup/SPIRV-Tools.git
pushd SPIRV-Tools
  git reset --hard c94501352d545e84c821ce031399e76d1af32d18
  python3 utils/git-sync-deps
  mkdir build
  pushd build
    cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DSPIRV_WERROR=OFF  -DCMAKE_CXX_FLAGS="-w" ..
    # Build something minimal to ensure all header files get generated.
    ninja SPIRV-Tools-static
  popd
popd
FILES=""
for f in `ls SPIRV-Tools/source/val/*.cpp`
do
    FILES="${FILES} ${DREDD_ROOT}/${f}"
done
${DREDD_ROOT}/third_party/clang+llvm-13.0.1/bin/dredd -p ${DREDD_ROOT}/SPIRV-Tools/build/compile_commands.json ${FILES}
pushd SPIRV-Tools/build
  ninja test_val_abcde test_val_capability test_val_fghijklmnop test_val_limits test_val_stuvw
  ./test/val/test_val_abcde
  ./test/val/test_val_capability
  ./test/val/test_val_fghijklmnop
  ./test/val/test_val_stuvw
popd

echo "LLVM: check that InstCombine builds after mutation"
date

git clone --branch llvmorg-14.0.6 --depth 1 https://github.com/llvm/llvm-project.git
pushd llvm-project
  mkdir build
  pushd build
    cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_CXX_FLAGS="-w" ../llvm
    # Build something minimal to ensure all header files get generated.
    ninja LLVMCore
  popd
popd
FILES=""
for f in `ls llvm-project/llvm/lib/Transforms/InstCombine/*.cpp`
do
    FILES="${FILES} ${DREDD_ROOT}/${f}"
done
${DREDD_ROOT}/third_party/clang+llvm-13.0.1/bin/dredd -p ${DREDD_ROOT}/llvm-project/build/compile_commands.json ${FILES}
pushd llvm-project/build
  ninja LLVMInstCombine
popd

echo "Finished devbuild"
date
