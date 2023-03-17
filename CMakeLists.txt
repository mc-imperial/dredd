# Copyright 2022 The Dredd Project Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.13)

project(
  dredd
  VERSION 1.0
  DESCRIPTION "Dredd"
  LANGUAGES C CXX)

# Note: this is not an option; it is used to set the default value for some
# options below. It is set to ON if Dredd is the root project being built, and
# OFF otherwise.
set(DREDD_PROJECT_IS_ROOT OFF)
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(DREDD_PROJECT_IS_ROOT ON)
endif()

# Options (i.e. cache variables) defined via "option" (if ON/OFF) or "set" (if
# not ON/OFF):
set(DREDD_GOOGLETEST_REPO_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest/googletest
    CACHE PATH "Path to a https://github.com/google/googletest repo.")

set(DREDD_PROTOBUF_REPO_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf/protobuf
    CACHE PATH "Path to a https://github.com/protocolbuffers/protobuf repo.")

option(DREDD_WARNINGS_AS_ERRORS "Enable warnings as errors for Dredd targets."
       ${DREDD_PROJECT_IS_ROOT})

option(DREDD_WARNINGS_EXTRA
       "Enable extra warnings via compile flags for Dredd targets."
       ${DREDD_PROJECT_IS_ROOT})

option(DREDD_WARNING_SUPPRESSIONS
       "Enable suppression of specific warnings for Dredd targets."
       ${DREDD_PROJECT_IS_ROOT})

option(DREDD_BUILD_TESTING "Build tests." ${DREDD_PROJECT_IS_ROOT})

if(DREDD_PROJECT_IS_ROOT)
  # Set some global compiler flags.
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)

  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

  set(CMAKE_POSITION_INDEPENDENT_CODE ON)

  include(CTest)

endif()

# Third party targets.

set(LLVM_DIR "${CMAKE_SOURCE_DIR}/third_party/clang+llvm/lib/cmake/llvm")

# Unfortunately this disables this warning entirely. At present, cmake-lint does
# not support disabling just for the following line.

# cmake-lint: disable=C0103
set(Clang_DIR "${CMAKE_SOURCE_DIR}/third_party/clang+llvm/lib/cmake/clang")

if(NOT EXISTS "${LLVM_DIR}/LLVMConfig.cmake")
  message(FATAL_ERROR "Could not find LLVMConfig.cmake under ${LLVM_DIR}. \
Make sure you have downloaded a release of Clang/LLVM.")
endif()

if(NOT EXISTS "${Clang_DIR}/ClangConfig.cmake")
  message(FATAL_ERROR "Could not find ClangConfig.cmake under ${Clang_DIR}. \
Make sure you have downloaded a release of Clang/LLVM.")
endif()

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED)

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
include_directories(SYSTEM ${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

if(DREDD_BUILD_TESTING)
  add_subdirectory(third_party/googletest)
endif()

add_subdirectory(third_party/protobuf)

# The targets that follow are all Dredd targets. We now conditionally set some
# compile options related to warnings. Most of these will only be set if Dredd
# is the root project being built. Using "add_*" functions is generally
# discouraged, but is suitable for "global" compile options. These will
# propagate to subdirectories via add_subdirectory(...) and will not be PUBLIC
# compile options (i.e. they will NOT propagate to targets above this project
# that link with these targets.

if(DREDD_WARNINGS_AS_ERRORS)
  if(MSVC)
    add_compile_options(/WX)
  else()
    add_compile_options(-Werror)
  endif()
endif()

if(DREDD_WARNINGS_EXTRA)
  if(MSVC)
    add_compile_options(/Wall)
  else()
    add_compile_options(-Wall -Wextra -pedantic)
    if(CMAKE_CXX_COMPILER_ID STREQUAL Clang)
      add_compile_options(-Weverything)
    endif()
  endif()
endif()

if(DREDD_WARNING_SUPPRESSIONS)
  if(MSVC)
    add_compile_options(
      # Disable specific warnings:
      /wd4068 # warning C4068: unknown pragma
      /wd4820 # warning C4820: 'xxx': 'x' bytes padding added after data member
              # 'xxx'
      /wd4191 # warning C4191: 'reinterpret_cast': unsafe conversion from 'x' to
              # 'x'
      /wd4625 # copy constructor was implicitly defined as deleted
      /wd5026 # move constructor was implicitly defined as deleted
      /wd4626 # assignment operator was implicitly defined as deleted
      /wd5027 # move assignment operator was implicitly defined as deleted
      /wd4514 # warning C4514: 'x': unreferenced inline function has been
              # removed
      /wd4711 # warning C4711: function 'x' selected for automatic inline
              # expansion (informational)
      /wd4710 # warning C4710: 'x': function not inlined (informational)
      /wd4996 # warning C4996: 'getenv': This function or variable may be
              # unsafe.
      /wd5045 # warning C5045: Compiler will insert Spectre mitigation for
              # memory load if /Qspectre switch specified
      /wd4061 # warning C4061: enumerator 'identifier' in switch of enum
              # 'enumeration' is not explicitly handled by a case label
      /wd4868 # warning C4868: compiler may not enforce left-to-right evaluation
              # order in braced initializer list
      /wd4702 # warning C4702: unrachable code.
      /wd5262 # warning C5262: implicit fall-through.
    )
  else()
    add_compile_options(
      -Wno-unknown-pragmas -Wno-unknown-warning-option -Wno-c++98-compat
      -Wno-c++98-compat-pedantic -Wno-padded -Wno-switch-enum)
  endif()
endif()

# Disable RTTI, as LLVM releases are not built with RTTI
if(MSVC)

else()
  add_compile_options(-fno-rtti)
endif()

add_subdirectory(src/libdredd)

if(DREDD_BUILD_TESTING)
  add_subdirectory(src/libdreddtest)
endif()

add_subdirectory(src/dredd)
