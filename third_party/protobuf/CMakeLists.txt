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

#
# Project: protobuf
#
# Provides targets: protobuf::protoc, protobuf::libprotobuf.
#
if(NOT TARGET protobuf)
  if(NOT IS_DIRECTORY ${DREDD_PROTOBUF_REPO_DIR})
    message(
      FATAL_ERROR "Could not find protobuf at ${DREDD_PROTOBUF_REPO_DIR}. "
                  "Try fetching submodules or set DREDD_PROTOBUF_REPO_DIR.")
  endif()

  # cmake-lint: disable=C0103
  set(protobuf_BUILD_TESTS
      OFF
      CACHE BOOL "Disable protobuf tests")
  set(protobuf_MSVC_STATIC_RUNTIME
      OFF
      CACHE BOOL "Do not build protobuf static runtime")

  add_subdirectory(${DREDD_PROTOBUF_REPO_DIR} EXCLUDE_FROM_ALL)
endif()
