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

if(DREDD_WARNING_SUPPRESSIONS)
  if(MSVC)

  else()
    add_compile_options(-Wno-global-constructors)
  endif()
endif()

add_executable(
  libdreddtest
  include_private/include/libdreddtest/gtest.h src/mutation_remove_stmt_test.cc
  src/mutation_replace_binary_operator_test.cc
  src/mutation_replace_expr_test.cc src/mutation_replace_unary_operator_test.cc)

target_link_libraries(libdreddtest PRIVATE libdredd gtest_main
                                           protobuf::libprotobuf)
target_include_directories(libdreddtest PRIVATE include_private/include
                                                ${CMAKE_BINARY_DIR}/src)

add_test(NAME libdreddtest COMMAND libdreddtest)
