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
CMAKE_OPTIONS=("-DCMAKE_OSX_ARCHITECTURES=arm64")

help | head

uname

export DREDD_LLVM_SUFFIX=""
DREDD_LLVM_TAG=$(./scripts/llvm_tag.sh)

case "$(uname)" in
"Linux")
  NINJA_OS="linux"
  export DREDD_LLVM_SUFFIX="-stock-clang"
  PYTHON="python3"
  DREDD_RELEASE_OS="${OS}"
  export CC=clang
  export CXX=clang++
  # Free up some space
  df -h
  sudo apt clean
  # shellcheck disable=SC2046
  docker rmi -f $(docker image ls -aq)
  sudo rm -rf /usr/share/dotnet /usr/local/lib/android /opt/ghc
  df -h

  # Install clang.
  DREDD_LLVM_TAG=$(./scripts/llvm_tag.sh)
  pushd ./third_party/clang+llvm
    curl -fsSL -o clang+llvm.tar.xz "https://github.com/llvm/llvm-project/releases/download/llvmorg-${DREDD_LLVM_TAG}/clang+llvm-${DREDD_LLVM_TAG}-x86_64-linux-gnu-ubuntu-22.04.tar.xz"
    tar xf clang+llvm.tar.xz
    mv clang+llvm-${DREDD_LLVM_TAG}-x86_64-linux-gnu-ubuntu-22.04/* .
    rm clang+llvm.tar.xz
  popd
  ;;

"Darwin")
  # Install clang.
  pushd ./third_party/clang+llvm
    curl -fsSL -o clang+llvm.zip "https://github.com/mc-imperial/build-clang/releases/download/llvmorg-${DREDD_LLVM_TAG}/build-clang-llvmorg-${DREDD_LLVM_TAG}-Mac_x64_Release.zip"
    unzip clang+llvm.zip
    rm clang+llvm.zip
  popd
  NINJA_OS="mac"
  PYTHON="python3"
  DREDD_RELEASE_OS="Mac"
  ;;

"MINGW"*|"MSYS_NT"*)
  NINJA_OS="win"
  PYTHON="python"
  DREDD_RELEASE_OS="Windows"
  CMAKE_OPTIONS+=("-DCMAKE_C_COMPILER=cl.exe" "-DCMAKE_CXX_COMPILER=cl.exe")
  choco install zip
  # Install clang.
  pushd ./third_party/clang+llvm
    curl -fsSL -o clang+llvm.zip "https://github.com/mc-imperial/build-clang/releases/download/llvmorg-${DREDD_LLVM_TAG}/build-clang-llvmorg-${DREDD_LLVM_TAG}-Windows_x64_Release.zip"
    unzip clang+llvm.zip
    rm clang+llvm.zip
  popd
  ;;

*)
  echo "Unknown OS"
  exit 1
  ;;
esac

export PATH="$(pwd)/third_party/clang+llvm/bin:${HOME}/bin:$PATH"

mkdir -p "${HOME}/bin"

pushd "${HOME}/bin"

# Install github-release-retry.
"${PYTHON}" -m pip install --user 'github-release-retry==1.*'

# Install ninja.
curl -fsSL -o ninja-build.zip "https://github.com/ninja-build/ninja/releases/download/v1.9.0/ninja-${NINJA_OS}.zip"
unzip ninja-build.zip

ls

popd

CMAKE_GENERATOR="Ninja"
CMAKE_BUILD_TYPE="${CONFIG}"

BUILD_DIR="b_${CONFIG}"

mkdir -p "${BUILD_DIR}"
pushd "${BUILD_DIR}"
cmake .. -G "${CMAKE_GENERATOR}" "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}" "${CMAKE_OPTIONS[@]}"
cmake --build . --config "${CMAKE_BUILD_TYPE}"
popd

# Get the files actually needed for the release: the dredd executable, and the header files that ship with Clang
mkdir -p dredd/bin
cp "${BUILD_DIR}/src/dredd/dredd" dredd/bin/dredd
mkdir -p "dredd/lib/clang/${DREDD_LLVM_TAG}"
cp -r "third_party/clang+llvm/lib/clang/${DREDD_LLVM_TAG}/include" "dredd/lib/clang/${DREDD_LLVM_TAG}"
DREDD_ZIP_NAME="dredd-${DREDD_RELEASE_OS}-${CONFIG}.zip"
zip -r "${DREDD_ZIP_NAME}" dredd

# We do not use the GITHUB_TOKEN provided by GitHub Actions.
# We cannot set enviroment variables or secrets that start with GITHUB_ in .yml files,
# but the github-release-retry tool requires GITHUB_TOKEN, so we set it here.
export GITHUB_TOKEN="${GH_TOKEN}"

DESCRIPTION="$(echo -e "Automated build for dredd revision ${GITHUB_SHA}.")"

"${PYTHON}" -m github_release_retry.github_release_retry \
  --user "mc-imperial" \
  --repo "dredd" \
  --tag_name "0.2" \
  --target_commitish "${GITHUB_SHA}" \
  --body_string "${DESCRIPTION}" \
  "${DREDD_ZIP_NAME}"
