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
  PYTHON="python3"
  DREDD_RELEASE_OS="${OS}"

  sudo apt update
  sudo apt install -y llvm-17 clang-17 clang-tidy-17 clang-format-17 libclang-17-dev
  
  export CC=clang
  export CXX=clang++
  # Free up some space
  df -h
  sudo apt clean
  sudo rm -rf /usr/share/dotnet /usr/local/lib/android /opt/ghc
  df -h
  ;;

*)
  echo "Unknown OS"
  exit 1
  ;;
esac

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

cmake -S . -B ${BUILD_DIR} -G Ninja -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DDREDD_CLANG_LLVM_DIR=/usr/lib/llvm-17
cmake --build ${BUILD_DIR} --config ${CMAKE_BUILD_TYPE}

# Get the file actually needed for the release: the dredd executable
mkdir -p dredd/bin
cp "${BUILD_DIR}/src/dredd/dredd" dredd/bin/dredd
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
  --tag_name "1.0" \
  --target_commitish "${GITHUB_SHA}" \
  --body_string "${DESCRIPTION}" \
  "${DREDD_ZIP_NAME}"
