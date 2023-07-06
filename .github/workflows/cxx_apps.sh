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

export DREDD_LLVM_SUFFIX=""

case "$(uname)" in
"Linux")
  NINJA_OS="linux"
  export DREDD_LLVM_SUFFIX="-prebuilt-clang"
  df -h
  sudo swapoff -a
  sudo rm -f /swapfile
  sudo apt clean
  # shellcheck disable=SC2046
  docker rmi $(docker image ls -aq)
  df -h
  ;;

"Darwin")
  NINJA_OS="mac"
  SDKROOT=$(xcrun --show-sdk-path)
  export SDKROOT
  ;;

"MINGW"*|"MSYS_NT"*)
  NINJA_OS="win"
  CMAKE_OPTIONS+=("-DCMAKE_C_COMPILER=cl.exe" "-DCMAKE_CXX_COMPILER=cl.exe")
  choco install zip
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

# Install clang.
.github/workflows/install_clang.sh

DREDD_ROOT=$(pwd)

case "$(uname)" in
"Linux")
  # On Linux, build Dredd using the prebuilt version of Clang that has been downloaded
  export PATH="${DREDD_ROOT}/third_party/clang+llvm/bin:$PATH"
  export CC=clang
  export CXX=clang++
  which ${CC}
  which ${CXX}
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
popd

# Check that dredd works on some projects
DREDD_EXECUTABLE="${DREDD_ROOT}/third_party/clang+llvm/bin/dredd"
cp "${DREDD_ROOT}/build/src/dredd/dredd" "${DREDD_EXECUTABLE}"

case "$(uname)" in
"Linux")
  CMAKE_OPTIONS_FOR_COMPILING_MUTATED_CODE=("-DCMAKE_CXX_FLAGS=-w")
  ;;

"MINGW"*|"MSYS_NT"*)
  # Dredd can lead to object files with many sections, which can be too much for MSVC's default settings.
  # The /bigobj switch enables a larger number of sections.
  CMAKE_OPTIONS_FOR_COMPILING_MUTATED_CODE=("-DCMAKE_CXX_FLAGS=\"/w /bigobj /EHsc\"")
  ;;

"Darwin")
  # The Apple compiler does not use C++11 by default; Dredd requires at least this language version.
  CMAKE_OPTIONS_FOR_COMPILING_MUTATED_CODE=("-DCMAKE_CXX_FLAGS=\"-w -std=c++11\"")
  ;;

*)
  ;;
esac

echo "examples/simple/pi.cc: check that we can build the simple example"
date

${DREDD_EXECUTABLE} --mutation-info-file temp.json examples/simple/pi.cc
case "$(uname)" in
"Linux"|"Darwin")
  clang++ -std=c++11 examples/simple/pi.cc -o examples/simple/pi
  ;;

"MINGW"*|"MSYS_NT"*)
  cl examples/simple/pi.cc
  mv pi.exe examples/simple/    
  ;;

*)
  ;;
esac

diff --strip-trailing-cr <(./examples/simple/pi) <(echo "3.14159")

echo "examples/math: check that the tests pass after mutating the library"
date

pushd examples/math
  mkdir build
  pushd build
    cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "${CMAKE_OPTIONS_FOR_COMPILING_MUTATED_CODE[@]}" ..
    ../mutate.sh
    cmake --build .
    ./mathtest/mathtest
  popd
popd

echo "SPIRV-Tools validator: check that the tests pass after mutating the validator"
date

git clone https://github.com/KhronosGroup/SPIRV-Tools.git
pushd SPIRV-Tools
  git reset --hard c94501352d545e84c821ce031399e76d1af32d18
  python3 utils/git-sync-deps
  mkdir build
  pushd build
    cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DSPIRV_WERROR=OFF "${CMAKE_OPTIONS_FOR_COMPILING_MUTATED_CODE[@]}" ..
    # Build something minimal to ensure all header files get generated.
    ninja SPIRV-Tools-static
  popd
popd
FILES=()
for f in SPIRV-Tools/source/val/*.cpp
do
  [[ -e "$f" ]] || break
  FILES+=("${DREDD_ROOT}/${f}")
done
${DREDD_EXECUTABLE} --mutation-info-file temp.json -p "${DREDD_ROOT}/SPIRV-Tools/build/compile_commands.json" "${FILES[@]}"
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
    cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "${CMAKE_OPTIONS_FOR_COMPILING_MUTATED_CODE[@]}" ../llvm
    # Build something minimal to ensure all header files get generated.
    ninja LLVMCore
  popd
popd

FILES=()
for f in llvm-project/llvm/lib/Transforms/InstCombine/*.cpp
do
  [[ -e "$f" ]] || break
  FILES+=("${DREDD_ROOT}/${f}")
done
${DREDD_EXECUTABLE} --mutation-info-file temp.json -p "${DREDD_ROOT}/llvm-project/build/compile_commands.json" "${FILES[@]}"
pushd llvm-project/build
  ninja LLVMInstCombine
popd

echo "Finished"
date
