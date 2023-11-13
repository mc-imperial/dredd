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

*)
  echo "Unknown OS: only Linux is supported for the dev_build workflow"
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

export PATH="${DREDD_ROOT}/third_party/clang+llvm/bin:$PATH"

export CC=clang
export CXX=clang++

which ${CC}
which ${CXX}

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug

# Check that dredd works on some projects
DREDD_EXECUTABLE="${DREDD_ROOT}/third_party/clang+llvm/bin/dredd"
cp "${DREDD_ROOT}/build/src/dredd/dredd" "${DREDD_EXECUTABLE}"

echo "examples/simple/pi.cc: check that we can build the simple example"
date

${DREDD_EXECUTABLE} --mutation-info-file temp.json examples/simple/pi.cc
clang++ examples/simple/pi.cc -o examples/simple/pi
diff <(./examples/simple/pi) <(echo "3.14159")

echo "examples/math: check that the tests pass after mutating the library"
date

pushd examples/math
  cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  ./mutate.sh
  cmake --build build
  ./build/mathtest/mathtest
popd

echo "SPIRV-Tools validator: check that the tests pass after mutating the validator"
date

git clone https://github.com/KhronosGroup/SPIRV-Tools.git
pushd SPIRV-Tools
  git reset --hard c94501352d545e84c821ce031399e76d1af32d18
  python3 utils/git-sync-deps
  cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DSPIRV_WERROR=OFF -DCMAKE_CXX_FLAGS="-w"
  # Build something minimal to ensure all header files get generated.
  cmake --build build --target SPIRV-Tools-static
  FILES=()
  for f in source/val/*.cpp
  do
    [[ -e "$f" ]] || break
    FILES+=("${DREDD_ROOT}/SPIRV-Tools/${f}")
  done
  ${DREDD_EXECUTABLE} --mutation-info-file temp.json -p "${DREDD_ROOT}/SPIRV-Tools/build/compile_commands.json" "${FILES[@]}"
  cmake --build build --target test_val_abcde test_val_capability test_val_fghijklmnop test_val_limits test_val_stuvw
  ./build/test/val/test_val_abcde
  ./build/test/val/test_val_capability
  ./build/test/val/test_val_fghijklmnop
  ./build/test/val/test_val_stuvw
popd

echo "LLVM: check that InstCombine builds after mutation"
date

git clone --branch llvmorg-17.0.4 --depth 1 https://github.com/llvm/llvm-project.git
pushd llvm-project
  cmake -S llvm -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_CXX_FLAGS="-w"
  # Build something minimal to ensure all header files get generated.
  cmake --build build --target LLVMCore

  FILES=()
  for f in llvm/lib/Transforms/InstCombine/*.cpp
  do
    [[ -e "$f" ]] || break
    FILES+=("${DREDD_ROOT}/llvm-project/${f}")
  done
  ${DREDD_EXECUTABLE} --mutation-info-file temp.json -p "${DREDD_ROOT}/llvm-project/build/compile_commands.json" "${FILES[@]}"
  cmake --build build --target LLVMInstCombine
popd

echo "Finished"
date
