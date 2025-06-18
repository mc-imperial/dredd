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
  sudo apt install llvm-17 clang-17 clang-tidy-17 clang-format-17 libclang-17-dev
  # shellcheck disable=SC2046
  docker rmi $(docker image ls -aq)
  df -h
  ;;

*)
  echo "Unknown OS: only Linux is supported for the cxx_apps workflow"
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

echo "examples/simple/pi.cc: check that we can build the simple example"
date

${DREDD_EXECUTABLE} --mutation-info-file temp.json examples/simple/pi.cc
clang++ examples/simple/pi.cc -o examples/simple/pi
diff <(./examples/simple/pi) <(echo "3.14159")

echo "examples/math: check that the tests pass after mutating the library"
date

pushd examples/math
  # Make a copy of the source for purposes of mutant querying
  cp -r math math-original
  cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  cmake --build build
  ${DREDD_EXECUTABLE} -p build --mutation-info-file mutation-info.json math/src/*.cc
  ./build/mathtest/mathtest
  NUM_MUTANTS=`python3 ${DREDD_ROOT}/scripts/query_mutant_info.py mutation-info.json --largest-mutant-id`
  EXPECTED_NUM_MUTANTS=1119
  if [ ${NUM_MUTANTS} -ne ${EXPECTED_NUM_MUTANTS} ]
  then
     echo "Found ${NUM_MUTANTS} mutants when mutating the math library source code. Expected ${EXPECTED_NUM_MUTANTS}. If Dredd changed recently, the expected value may just need to be updated, if it still looks sensible. Otherwise, there is likely a problem."
     exit 1
  fi

  # Display info about every mutant, just to check that the script that displays mutant info does not error.
  for mutant in `seq 0 ${NUM_MUTANTS}`
  do
    python3 ${DREDD_ROOT}/scripts/query_mutant_info.py mutation-info.json --show-info-for-mutant ${mutant} --path-prefix-replacement ${DREDD_ROOT}/examples/math/math ${DREDD_ROOT}/examples/math/math-original > /dev/null
  done
popd

echo "examples/threaded: check that a simple concurrent program runs OK after mutation"
date

pushd examples/threaded
  # The CMake configuration enables thread sanitizer.
  cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  FILES=()
  for f in src/*.cc
  do
    [[ -e "$f" ]] || break
    FILES+=("${DREDD_ROOT}/examples/threaded/${f}")
  done
  ${DREDD_EXECUTABLE} --mutation-info-file temp.json -p "${DREDD_ROOT}/examples/threaded/build" "${FILES[@]}"
  cmake --build build
  # Check that the application runs correctly and that there are no data races.
  TSAN_OPTIONS=halt_on_error=1 ./build/threaded > threaded_output.txt
  ACTUAL_THREADED_OUTPUT=`cat threaded_output.txt`
  EXPECTED_THREADED_OUTPUT="8192"
  if [ ${ACTUAL_THREADED_OUTPUT} -ne ${EXPECTED_THREADED_OUTPUT} ]
  then
      echo "Output mismatch for threaded program after mutation. Expected ${EXPECTED_THREADED_OUTPUT} but got ${ACTUAL_THREADED_OUTPUT}"
      exit 1
  fi
  rm threaded_output.txt
popd

echo "SPIRV-Tools validator: check that the tests pass after mutating the validator"
date

git clone https://github.com/KhronosGroup/SPIRV-Tools.git
pushd SPIRV-Tools
  git reset --hard 2a238ed24dffd84fe3ed2e60d7aa5c28e2acf45a
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
  ${DREDD_EXECUTABLE} --mutation-info-file mutation-info.json -p "${DREDD_ROOT}/SPIRV-Tools/build" "${FILES[@]}"
  cmake --build build --target test_val_abcde test_val_capability test_val_fghijklmnop test_val_limits test_val_rstuvw
  ./build/test/val/test_val_abcde
  ./build/test/val/test_val_capability
  ./build/test/val/test_val_fghijklmnop
  ./build/test/val/test_val_rstuvw
  NUM_MUTANTS=`python3 ${DREDD_ROOT}/scripts/query_mutant_info.py mutation-info.json --largest-mutant-id`
  EXPECTED_NUM_MUTANTS=68191
  if [ ${NUM_MUTANTS} -ne ${EXPECTED_NUM_MUTANTS} ]
  then
     echo "Found ${NUM_MUTANTS} mutants when mutating the SPIR-V validator source code. Expected ${EXPECTED_NUM_MUTANTS}. If Dredd changed recently, the expected value may just need to be updated, if it still looks sensible. Otherwise, there is likely a problem."
     exit 1
  fi
popd

echo "LLVM: check that InstCombine builds after mutation"
date

git clone --branch llvmorg-17.0.4 --depth 1 https://github.com/llvm/llvm-project.git
pushd llvm-project
  cmake -S llvm -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_CXX_FLAGS="-w" -DCMAKE_BUILD_TYPE=Release
  # Build something minimal to ensure all header files get generated.
  cmake --build build --target LLVMCore

  FILES=()
  for f in llvm/lib/Transforms/InstCombine/*.cpp
  do
    [[ -e "$f" ]] || break
    FILES+=("${DREDD_ROOT}/llvm-project/${f}")
  done
  ${DREDD_EXECUTABLE} --mutation-info-file mutation-info.json -p "${DREDD_ROOT}/llvm-project/build" "${FILES[@]}"
  cmake --build build --target LLVMInstCombine
  NUM_MUTANTS=`python3 ${DREDD_ROOT}/scripts/query_mutant_info.py mutation-info.json --largest-mutant-id`
  EXPECTED_NUM_MUTANTS=93081
  if [ ${NUM_MUTANTS} -ne ${EXPECTED_NUM_MUTANTS} ]
  then
     echo "Found ${NUM_MUTANTS} mutants when mutating the LLVM source code. Expected ${EXPECTED_NUM_MUTANTS}. If Dredd changed recently, the expected value may just need to be updated, if it still looks sensible. Otherwise, there is likely a problem."
     exit 1
  fi
popd

echo "Finished"
date
