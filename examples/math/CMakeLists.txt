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

project(math)

# This can be used to disable the building of tests, which assume that the math
# example is located in a certain place in the Dredd repository. It is useful to
# be able to disable tests for this example so that its source code can be moved
# elsewhere and mutated as part of end-to-end testsing of Dredd.
set(DREDD_EXAMPLES_MATH_BUILD_TESTS
    ON
    CACHE BOOL "Enable building of tests?")

add_subdirectory(math)

if(DREDD_EXAMPLES_MATH_BUILD_TESTS)
  set(DREDD_GOOGLETEST_REPO_DIR
      "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/googletest/googletest")
  add_subdirectory(mathtest)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/googletest"
                   "${CMAKE_CURRENT_BINARY_DIR}/third_party/googletest")
endif()
