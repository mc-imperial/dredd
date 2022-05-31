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

##
## Project: googletest-distribution
##
## Provides targets: gtest, gtest_main.
##
if(NOT TARGET gtest)
    if(NOT IS_DIRECTORY ${DREDD_GOOGLETEST_REPO_DIR})
        message(
            FATAL_ERROR
            "Could not find googletest-distribution at ${DREDD_GOOGLETEST_REPO_DIR}. "
            "Try fetching submodules or set DREDD_GOOGLETEST_REPO_DIR.")
    endif()

    set(
        BUILD_GMOCK
        OFF
        CACHE
        BOOL
        "Builds the googlemock subproject"
    )

    set(
        INSTALL_GTEST
        OFF
        CACHE
        BOOL
        "Enable installation of googletest. (Projects embedding googletest may want to turn this OFF.)"
    )

    set(
        gtest_force_shared_crt
        ON
        CACHE
        BOOL
        "Use shared (DLL) run-time lib even when Google Test is built as static lib."
    )

    add_subdirectory(${DREDD_GOOGLETEST_REPO_DIR} EXCLUDE_FROM_ALL)
endif()
