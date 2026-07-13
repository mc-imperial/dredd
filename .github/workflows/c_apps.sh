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
  sudo apt install -y \
       llvm-17 \
       clang-17 \
       clang-tidy-17 \
       clang-format-17 \
       libclang-17-dev \
       llvm-20-dev \
       libllvm20 \
       meson-1.7 \
       ninja-build \
       jq \
       glslang-tools \
       pkg-config \
       bison \
       flex \
       libdrm-dev
  python3 -m pip install --upgrade mako packaging pyyaml
  python3 -c "import packaging, mako, yaml; print('py deps ok')"

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


echo "Mesa"
date

git clone --depth 1 --branch mesa-26.1.4 https://gitlab.freedesktop.org/mesa/mesa.git mesa-26.1.4

pushd mesa-26.1.4

LLVM_CONFIG=llvm-config-20 CC=clang-17 CXX=clang++-17 meson setup builddir/ \
  -Dbuildtype=debugoptimized \
  -Dvulkan-drivers=swrast \
  -Dgallium-drivers=llvmpipe \
  -Dplatforms= \
  -Dllvm=enabled \
  -Dshared-llvm=enabled \
  -Dopengl=false \
  -Dgles1=disabled \
  -Dgles2=disabled \
  -Dglx=disabled \
  -Degl=disabled \
  -Dgbm=disabled \
  -Dc_args='-Wno-error=return-type'
meson compile -C builddir/
pushd builddir
"${DREDD_EXECUTABLE}" \
    --mutation-info-file=mutation-info.json \
    -p . \
    $(jq -r '.[].file' compile_commands.json | grep -E '/(compiler/(spirv|nir)/[^/]+\.(c|cc|cpp))' | sort)
popd
meson compile -C builddir/
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
