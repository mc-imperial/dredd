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

export PATH="${HOME}/bin:$PATH"
mkdir -p "${HOME}/bin"
pushd "${HOME}/bin"
  # Install ninja.
  curl -fsSL -o ninja-build.zip "https://github.com/ninja-build/ninja/releases/download/v1.11.0/ninja-${NINJA_OS}.zip"
  unzip ninja-build.zip
  ls
popd

DREDD_LLVM_TAG=$(./scripts/llvm_tag.sh)

# Install clang.
CLANG_VERSION=clang+llvm-${DREDD_LLVM_TAG}
pushd ./third_party/"${CLANG_VERSION}"
curl -fsSL -o "${CLANG_VERSION}.zip" "https://github.com/mc-imperial/build-clang/releases/download/llvmorg-${DREDD_LLVM_TAG}/build-clang-llvmorg-${DREDD_LLVM_TAG}-Linux_x64_Release.zip"
unzip "${CLANG_VERSION}.zip"
rm "${CLANG_VERSION}.zip"
popd

export PATH="./third_party/clang+llvm-${DREDD_LLVM_TAG}/bin:$PATH"

export CC=clang
export CXX=clang++

which ${CC}
which ${CXX}

mkdir -p build
pushd build
  cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Debug
  cmake --build . --config Debug
  cmake -DCMAKE_INSTALL_PREFIX=./install -DBUILD_TYPE=Debug -P cmake_install.cmake
popd

# Check that dredd works on some projects
DREDD_ROOT=$(pwd)
DREDD_EXECUTABLE="${DREDD_ROOT}/third_party/clang+llvm-${DREDD_LLVM_TAG}/bin/dredd"
cp "${DREDD_ROOT}/build/src/dredd/dredd" "${DREDD_EXECUTABLE}"

echo "examples/simple/pi.cc: check that we can build the simple example"
date

${DREDD_EXECUTABLE} --mutation-info-file temp.json examples/simple/pi.cc
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
for f in SPIRV-Tools/source/val/*.cpp
do
  [[ -e "$f" ]] || break
  FILES="${FILES} ${DREDD_ROOT}/${f}"
done
${DREDD_EXECUTABLE} --mutation-info-file temp.json -p "${DREDD_ROOT}/SPIRV-Tools/build/compile_commands.json" "${FILES}"
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

for f in llvm-project/llvm/lib/Transforms/InstCombine/*.cpp
do
  [[ -e "$f" ]] || break
  FILES="${FILES} ${DREDD_ROOT}/${f}"
done
${DREDD_EXECUTABLE} --mutation-info-file temp.json -p "${DREDD_ROOT}/llvm-project/build/compile_commands.json ${FILES}"
pushd llvm-project/build
  ninja LLVMInstCombine
popd

echo "Finished"
date
